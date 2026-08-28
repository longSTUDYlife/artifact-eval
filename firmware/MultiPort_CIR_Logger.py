"""
Multi-Port CIR Real-Time Acquisition and Visualization System (Python Version)
Supports any number of antennas/ports, no thread limit
"""

import serial
import os, numpy as np
import threading
import queue
import time
from collections import deque
from datetime import datetime
import csv
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation
from scipy import signal as sp_signal
import struct
import multiprocessing as mp
from multiprocessing import Process, Queue, Manager

# ================== Configuration Parameters ==================
PORT_COUNT = 8  # Number of antennas/ports
COM_PORTS = ['COM11', 'COM12', 'COM22', 'COM13', 'COM18', 'COM19', 'COM20', 'COM16']

BAUD_RATE = 115200
HEADER_SIZE = 6
FOOTER_SIZE = 4
DIAG_SIZE = 14
CIR_SIZE = 400
MAX_QUEUE_LEN = 200

# Physical parameters
C = 3e8
FC = 3494.4e6
LAMBDA = C / FC
FS = 64e9
UPSAMPLE_FACTOR = 64
D = LAMBDA / 2
FRAME_RATE = 83
ORIGIN_START_IDX = 699
WIN_RADIUS = 83
RD_WINDOW = 83
WINDOW_LEFT = 10 * UPSAMPLE_FACTOR
WINDOW_RIGHT = 40 * UPSAMPLE_FACTOR
NUM_CIR_POINTS = 100

# ⚠️ Reduce complexity to improve frame rate
BEAMFORMING_SKIP_FRAMES = 1  # Beamforming displays every N frames (1=no skip)
RD_SKIP_FRAMES = 1  # RD displays every N frames
THETA_RESOLUTION = 2  # Angle resolution (degrees) - changed from 1 to 2 degrees to reduce computation
RANGE_BINS_LIMIT = 300  # Limit number of range bins

# ================== CRC16 Calculation ==================
def compute_crc16(data):
    """Calculate CRC16 checksum"""
    crc = 0xFFFF
    poly = 0xA001
    
    for byte in data:
        crc ^= byte
        for _ in range(8):
            if crc & 1:
                crc = (crc >> 1) ^ poly
            else:
                crc >>= 1
    return crc

# ================== Frame Parsing ==================

def read_complex_csv(filepath):
    """Read MATLAB-style complex CSV file (format: a+bi)"""
    complex_values = []
    with open(filepath, 'r') as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            # Replace 'i' with 'j' (Python complex format)
            line = line.replace('i', 'j')
            try:
                val = complex(line)
                complex_values.append(val)
            except:
                pass
    return np.array(complex_values, dtype=np.complex128)

class Frame:
    """CIR frame data structure"""
    def __init__(self):
        self.ok = False
        self.seq = 0
        self.payload_length = 0
        self.packet_type = 0
        self.max_noise = 0
        self.first_path_amp1 = 0
        self.std_noise = 0
        self.first_path_amp2 = 0
        self.first_path_amp3 = 0
        self.rx_pream_count = 0
        self.first_path_raw = 0.0
        self.fp_float = 0.0
        self.cir_real_imag = []
        self.amplitude = []

def parse_one_frame(frame_data):
    """Parse a single CIR frame"""
    f = Frame()
    
    if len(frame_data) < (HEADER_SIZE + DIAG_SIZE + CIR_SIZE + FOOTER_SIZE):
        return f
    
    if frame_data[0] != 0xAA:  # 170
        return f
    
    if frame_data[HEADER_SIZE + DIAG_SIZE + CIR_SIZE + 2] != 0x55:  # 85
        return f
    
    # Parse header
    payload_length = frame_data[2] + frame_data[3] * 256
    if payload_length != (DIAG_SIZE + CIR_SIZE):
        return f
    
    # Verify CRC
    payload_end = HEADER_SIZE + payload_length
    chk_recv = frame_data[payload_end] + frame_data[payload_end + 1] * 256
    chk_calc = compute_crc16(frame_data[:payload_end])
    
    if chk_recv != chk_calc:
        return f
    
    # Extract fields
    f.seq = frame_data[5]
    f.payload_length = payload_length
    f.packet_type = frame_data[4]
    
    # Parse diagnostic data
    payload = frame_data[HEADER_SIZE:HEADER_SIZE + payload_length]
    diag_vals = []
    for i in range(7):
        idx = i * 2
        val = payload[idx] + payload[idx + 1] * 256
        diag_vals.append(val)
    
    f.max_noise = diag_vals[0]
    f.first_path_amp1 = diag_vals[1]
    f.std_noise = diag_vals[2]
    f.first_path_amp2 = diag_vals[3]
    f.first_path_amp3 = diag_vals[4]
    f.rx_pream_count = diag_vals[5]
    
    # Parse first path
    fp_raw = diag_vals[6]
    int_part = fp_raw >> 6
    frac = fp_raw & 0x3F
    f.fp_float = int_part + frac / 64.0
    f.first_path_raw = f.fp_float
    
    # Parse CIR data
    cir_data = payload[DIAG_SIZE:DIAG_SIZE + CIR_SIZE]
    cir_real_imag = []
    
    for k in range(CIR_SIZE // 4):
        base_idx = 4 * k
        re_val = struct.unpack('<h', bytes(cir_data[base_idx:base_idx + 2]))[0]
        im_val = struct.unpack('<h', bytes(cir_data[base_idx + 2:base_idx + 4]))[0]
        cir_real_imag.extend([float(re_val), float(im_val)])
    
    f.cir_real_imag = cir_real_imag
    
    # Calculate amplitude
    denom = f.rx_pream_count if f.rx_pream_count > 0 else 1
    amplitude = []
    for k in range(100):
        re = cir_real_imag[2 * k]
        im = cir_real_imag[2 * k + 1]
        amp = np.sqrt(re**2 + im**2) / denom
        amplitude.append(amp)
    
    f.amplitude = amplitude
    f.ok = True
    return f

def parse_frames_from_buffer(buffer):
    """Parse all complete frames from buffer"""
    frames = []
    idx = 0
    buf_len = len(buffer)
    
    while True:
        if (buf_len - idx) < (HEADER_SIZE + FOOTER_SIZE + DIAG_SIZE + CIR_SIZE):
            break
        
        if buffer[idx] != 0xAA:
            idx += 1
            continue
        
        if (buf_len - idx) < HEADER_SIZE:
            break
        
        payload_length = buffer[idx + 2] + buffer[idx + 3] * 256
        frame_len = HEADER_SIZE + payload_length + FOOTER_SIZE
        
        if (buf_len - idx) < frame_len:
            break
        
        frame_data = buffer[idx:idx + frame_len]
        f = parse_one_frame(frame_data)
        
        if f.ok:
            frames.append(f)
            idx += frame_len
        else:
            idx += 1
    
    return frames, buffer[idx:]

# ================== Circular Buffer ==================
class FrameBuffer:
    """Thread-safe circular buffer"""
    def __init__(self, max_len):
        self.buffer = [None] * max_len
        self.max_len = max_len
        self.write_idx = 0
        self.count = 0
        self.lock = threading.Lock()
    
    def push(self, frame):
        with self.lock:
            self.write_idx = (self.write_idx % self.max_len)
            self.buffer[self.write_idx] = frame
            self.write_idx += 1
            self.count = min(self.count + 1, self.max_len)
    
    def latest(self):
        with self.lock:
            if self.count == 0:
                return None
            idx = (self.write_idx - 1) % self.max_len
            return self.buffer[idx]
    
    def get_last_n(self, n):
        with self.lock:
            if self.count == 0:
                return []
            n = min(n, self.count)
            frames = []
            for i in range(n):
                idx = (self.write_idx - n + i) % self.max_len
                frames.append(self.buffer[idx])
            return frames

# ================== Serial Acquisition Thread ==================
class SerialWorker(threading.Thread):
    """Acquisition thread for a single serial port"""
    def __init__(self, port_idx, com_port, frame_buffer, data_queue, csv_file, save_control):
        super().__init__(daemon=True)
        self.port_idx = port_idx
        self.com_port = com_port
        self.frame_buffer = frame_buffer
        self.data_queue = data_queue
        self.csv_file = csv_file
        self.save_control = save_control
        self.running = True
        self.frame_count = 0
        self.error_logged = False
        
    def run(self):
        print(f"[Worker {self.port_idx}] Attempting to open serial port: {self.com_port}")
        
        try:
            # Open serial port
            try:
                ser = serial.Serial(self.com_port, BAUD_RATE, timeout=0.1)
                print(f"[Worker {self.port_idx}] ✓ Serial port opened successfully: {self.com_port}")
            except serial.SerialException as e:
                print(f"[Worker {self.port_idx}] ✗ Failed to open serial port: {self.com_port}")
                print(f"                     Error details: {e}")
                return
            
            buffer = bytearray()
            csv_writer = None
            csv_file_handle = None
            
            print(f"[Worker {self.port_idx}] Starting to listen for data...")
            last_data_time = time.time()
            no_data_warned = False
            last_save_state = self.save_control['saving']
            
            while self.running:
                try:
                    # Check if save state has changed
                    if self.save_control['saving'] != last_save_state:
                        if self.save_control['saving']:
                            # Start saving - open CSV file
                            csv_file_handle = open(self.csv_file, 'w', newline='')
                            csv_writer = csv.writer(csv_file_handle)
                            # Write header
                            header = ['Sequence', 'PayloadLength', 'PacketType', 'maxNoise',
                                     'firstPathAmp1', 'stdNoise', 'firstPathAmp2', 'firstPathAmp3',
                                     'rxPreamCount', 'firstPath']
                            for i in range(100):
                                header.extend([f'CIR_real_{i}', f'CIR_imag_{i}'])
                            csv_writer.writerow(header)
                            csv_file_handle.flush()
                            print(f"[Worker {self.port_idx}] Started writing to CSV: {self.csv_file}")
                        else:
                            # Stop saving - close CSV file
                            if csv_file_handle is not None:
                                csv_file_handle.close()
                                csv_file_handle = None
                                csv_writer = None
                                print(f"[Worker {self.port_idx}] Stopped writing to CSV")
                        
                        last_save_state = self.save_control['saving']
                    
                    # Read serial port data
                    if ser.in_waiting > 0:
                        new_data = ser.read(ser.in_waiting)
                        buffer.extend(new_data)
                        last_data_time = time.time()
                        no_data_warned = False
                        
                        # Parse frames
                        frames, buffer = parse_frames_from_buffer(buffer)
                        
                        for frame in frames:
                            # Push to buffer (always)
                            self.frame_buffer.push(frame)
                            
                            # Notify main thread (always)
                            self.data_queue.put((self.port_idx, frame))
                            self.frame_count += 1
                            
                            # Write to CSV (only when saving)
                            if self.save_control['saving'] and csv_writer is not None:
                                row = [frame.seq, frame.payload_length, frame.packet_type,
                                      frame.max_noise, frame.first_path_amp1, frame.std_noise,
                                      frame.first_path_amp2, frame.first_path_amp3,
                                      frame.rx_pream_count, frame.first_path_raw]
                                row.extend(frame.cir_real_imag)
                                csv_writer.writerow(row)
                                if self.frame_count % 100 == 0:
                                    csv_file_handle.flush()
                    else:
                        # Warn if no data for more than 5 seconds
                        if not no_data_warned and (time.time() - last_data_time) > 5:
                            print(f"[Worker {self.port_idx}] ⚠ Warning: {self.com_port} no data for more than 5 seconds")
                            no_data_warned = True
                    
                    time.sleep(0.0001)
                
                except Exception as e:
                    if not self.error_logged:
                        print(f"[Worker {self.port_idx}] Processing error: {e}")
                        self.error_logged = True
            
            # Clean up CSV file
            if csv_file_handle is not None:
                csv_file_handle.close()
            
            ser.close()
            print(f"[Worker {self.port_idx}] Serial port closed: {self.com_port}")
            
        except Exception as e:
            print(f"[Worker {self.port_idx}] Critical error: {e}")
            import traceback
            traceback.print_exc()
    
    def stop(self):
        self.running = False

# ================== Beamforming Visualization Process ==================
def beamforming_process(frame_buffers_shared, port_count, data_ready_event):
    """Independent process for Beamforming visualization"""
    print(f"[Beamforming Process] Started PID={mp.current_process().pid}")
    
    class BeamformingVisualizer:
        def __init__(self, port_count):
            self.port_count = port_count
            self.colorbar = None
            self.frame_times = []
            
            # Create figure
            self.fig = plt.figure(figsize=(14, 10))
            self.fig.canvas.manager.set_window_title(f'CIR + Beamforming ({port_count} Ports)')
            
            # CIR amplitude plot
            self.ax_cir = plt.subplot2grid((3, 1), (0, 0), rowspan=1)
            self.ax_cir.set_title(f'Aligned CIR Amplitude (All {port_count} Ports)')
            self.ax_cir.set_xlabel('Sample Index (Upsampled)')
            self.ax_cir.set_ylabel('Amplitude')
            
            self.lines_cir = []
            colors = plt.cm.tab10(np.linspace(0, 1, port_count))
            for m in range(port_count):
                line, = self.ax_cir.plot([], [], linewidth=1.5, 
                                         label=f'Port {m+1}', color=colors[m])
                self.lines_cir.append(line)
            self.ax_cir.legend(loc='best', ncol=4)
            
            # Beamforming plot
            self.ax_bf = plt.subplot2grid((3, 1), (1, 0), rowspan=2, projection='polar')
            self.ax_bf.set_title(f'Range-Azimuth Beamforming ({port_count} Antennas)', pad=30, fontsize=12)
            
            self.theta_scan = np.arange(90, -91, -1)
            self.theta_rad = np.deg2rad(self.theta_scan)
            self.steering = np.exp(-1j * 2 * np.pi / LAMBDA * 0.04064 * 
                                  np.arange(port_count)[:, None] * np.sin(self.theta_rad))
            
            self.max_range = 7.0
            self.ax_bf.set_theta_zero_location('N')
            self.ax_bf.set_theta_direction(-1)
            self.ax_bf.set_thetamin(-90)
            self.ax_bf.set_thetamax(90)
            self.ax_bf.set_ylim(0, self.max_range)
            self.ax_bf.set_ylabel('Range (m)', labelpad=30)
            self.fig.tight_layout(pad=2.0)
            
            self.surf = None
        
        def update(self, frame_num):
            """Read from shared data and update"""
            current_time = time.time()
            self.frame_times.append(current_time)
            self.frame_times = [t for t in self.frame_times if current_time - t < 1.0]
            actual_fps = len(self.frame_times)
            
            if frame_num % 10 == 0:
                self.ax_cir.set_title(f'Aligned CIR Amplitude (All {self.port_count} Ports) - {actual_fps} FPS')
            
            # Read latest data from shared queue
            if not frame_buffers_shared.empty():
                try:
                    data = frame_buffers_shared.get_nowait()
                    frames = data['frames']
                    valid_ports = data['valid_ports']
                    cir_mat = data['cir_mat']
                    rx = data['rx']
                    active_steering = data['active_steering']
                    fixed_len = data['fixed_len']
                    ref_lde = data['ref_lde']
                    
                    # Update CIR plot
                    x_vals = np.arange(fixed_len)
                    for m, port_idx in enumerate(valid_ports):
                        aligned_mag = np.abs(cir_mat[m, :])
                        self.lines_cir[port_idx].set_data(x_vals, aligned_mag)
                    
                    for m in range(self.port_count):
                        if m not in valid_ports:
                            self.lines_cir[m].set_data([], [])
                    
                    if frame_num % 10 == 0:
                        self.ax_cir.set_xlim(0, fixed_len)
                        if np.max(np.abs(cir_mat)) > 0:
                            self.ax_cir.set_ylim(0, np.max(np.abs(cir_mat)) * 1.1)
                    
                    # Calculate RA map
                    valid_idx = ((np.arange(fixed_len) - ref_lde) / FS * C / 2) >= 0
                    valid_idx = valid_idx & (((np.arange(fixed_len) - ref_lde) / FS * C / 2) <= self.max_range)
                    
                    if np.sum(valid_idx) > 0:
                        rx_valid = rx[:, valid_idx]
                        r_axis = ((np.arange(np.sum(valid_idx))) / FS * C / 2)
                        
                        RA_map = np.zeros((len(r_axis), len(self.theta_scan)))
                        for i in range(len(r_axis)):
                            x = rx_valid[:, i]
                            RA_map[i, :] = np.abs(active_steering.T.conj() @ x) ** 2
                        
                        if np.max(RA_map) > 0:
                            RA_map = RA_map / np.max(RA_map)
                        
                        r_thresh = 2
                        RA_map[r_axis < r_thresh, :] = 0
                        if np.max(RA_map) > 0:
                            RA_map = RA_map / np.max(RA_map)
                        
                        theta_polar = np.deg2rad(self.theta_scan)
                        THETA, R = np.meshgrid(theta_polar, r_axis)
                        
                        if self.surf is None:
                            self.surf = self.ax_bf.pcolormesh(
                                THETA, R, RA_map,
                                cmap='jet',
                                vmin=0,
                                vmax=1,
                                shading='auto'
                            )
                            self.colorbar = plt.colorbar(self.surf, ax=self.ax_bf, pad=0.1)
                        else:
                            self.surf.set_array(RA_map.ravel())
                            if np.max(RA_map) > 0:
                                self.surf.set_clim(0, 1)
                except:
                    pass
            
            return self.lines_cir + ([self.surf] if self.surf else [])
    
    viz = BeamformingVisualizer(port_count)
    anim = FuncAnimation(viz.fig, viz.update, interval=100, blit=False, cache_frame_data=False)
    plt.show()

# ================== Range-Doppler Visualization Process ==================
def rangedoppler_process(frame_buffers_shared, port_count, data_ready_event):
    """Independent process for Range-Doppler visualization"""
    print(f"[RangeDoppler Process] Started PID={mp.current_process().pid}")
    
    class RangeDopplerVisualizer:
        def __init__(self, port_count):
            self.port_count = port_count
            self.velocity_axis = ((np.arange(RD_WINDOW) - RD_WINDOW / 2) * 
                                 (LAMBDA * FRAME_RATE / 2 / RD_WINDOW))
            self.fixed_max_len = WINDOW_LEFT + WINDOW_RIGHT + 1
            self.range_axis = ((np.arange(self.fixed_max_len) - WINDOW_LEFT) / FS * C / 2)
            
            rows = int(np.ceil(np.sqrt(port_count)))
            cols = int(np.ceil(port_count / rows))
            
            self.fig, self.axes = plt.subplots(rows, cols, figsize=(12, 8))
            self.fig.canvas.manager.set_window_title(f'Range-Doppler Maps ({port_count} Antennas)')
            
            if port_count == 1:
                self.axes = np.array([self.axes])
            self.axes = self.axes.flatten()
            
            self.images = []
            for m in range(port_count):
                self.axes[m].set_title(f'Antenna {m+1}')
                self.axes[m].set_xlabel('Velocity (m/s)')
                self.axes[m].set_ylabel('Range (m)')
                
                img = self.axes[m].imshow(
                    np.zeros((self.fixed_max_len, RD_WINDOW)), 
                    aspect='auto', 
                    cmap='jet', 
                    origin='lower',
                    extent=[self.velocity_axis[0], self.velocity_axis[-1], 
                           self.range_axis[0], self.range_axis[-1]],
                    vmin=0, vmax=1
                )
                self.images.append(img)
                plt.colorbar(img, ax=self.axes[m])
            
            for m in range(port_count, len(self.axes)):
                self.axes[m].axis('off')
        
        def update(self, frame_num):
            # Read RD data from shared queue
            if not frame_buffers_shared.empty():
                try:
                    rd_data = frame_buffers_shared.get_nowait()
                    for m in range(self.port_count):
                        if m < len(rd_data):
                            rd_map = rd_data[m]
                            self.images[m].set_data(rd_map)
                            if np.max(rd_map) > 0:
                                self.images[m].set_clim(0, np.max(rd_map))
                except:
                    pass
            
            return self.images
    
    viz = RangeDopplerVisualizer(port_count)
    anim = FuncAnimation(viz.fig, viz.update, interval=1000, blit=False, cache_frame_data=False)
    plt.show()
    """Beamforming visualization"""
    def __init__(self, frame_buffers, port_count):
        self.frame_buffers = frame_buffers
        self.port_count = port_count
        self.colorbar = None
        self.last_update_time = time.time()
        self.frame_times = []
        
        # Create figure - adjust layout to make semicircle larger
        self.fig = plt.figure(figsize=(14, 10))
        self.fig.canvas.manager.set_window_title(f'CIR + Beamforming ({port_count} Ports)')
        
        # CIR amplitude plot - occupies top 1/3
        self.ax_cir = plt.subplot2grid((3, 1), (0, 0), rowspan=1)
        self.ax_cir.set_title(f'Aligned CIR Amplitude (All {port_count} Ports)')
        self.ax_cir.set_xlabel('Sample Index (Upsampled)')
        self.ax_cir.set_ylabel('Amplitude')
        
        self.lines_cir = []
        colors = plt.cm.tab10(np.linspace(0, 1, port_count))
        for m in range(port_count):
            line, = self.ax_cir.plot([], [], linewidth=1.5, 
                                     label=f'Port {m+1}', color=colors[m])
            self.lines_cir.append(line)
        self.ax_cir.legend(loc='best', ncol=4)
        
        # Beamforming plot - occupies bottom 2/3, polar semicircle display
        self.ax_bf = plt.subplot2grid((3, 1), (1, 0), rowspan=2, projection='polar')
        self.ax_bf.set_title(f'Range-Azimuth Beamforming ({port_count} Antennas)', pad=30, fontsize=12)
        
        # Angles and steering matrix
        self.theta_scan = np.arange(90, -91, -1)  # From top to bottom
        self.theta_rad = np.deg2rad(self.theta_scan)
        self.steering = np.exp(1j * 2 * np.pi / LAMBDA * 0.04064 * 
                              np.arange(port_count)[:, None] * np.sin(self.theta_rad))
        
        self.max_range = 7.0
        
        # Set polar coordinate system to semicircle (upper semicircle)
        self.ax_bf.set_theta_zero_location('N')  # 0 degrees at top
        self.ax_bf.set_theta_direction(-1)  # Clockwise
        self.ax_bf.set_thetamin(-90)  # Left boundary
        self.ax_bf.set_thetamax(90)   # Right boundary
        self.ax_bf.set_ylim(0, self.max_range)
        
        # Add range label
        self.ax_bf.set_ylabel('Range (m)', labelpad=30)
        
        # Adjust polar plot position to be closer to top
        self.fig.tight_layout(pad=2.0)
        
        self.surf = None  # Create later
    
    def update(self, frame_num):
        """Update visualization"""
        # Calculate actual frame rate
        current_time = time.time()
        self.frame_times.append(current_time)
        self.frame_times = [t for t in self.frame_times if current_time - t < 1.0]
        actual_fps = len(self.frame_times)
        
        # Update title every 10 frames
        if frame_num % 10 == 0:
            self.ax_cir.set_title(f'Aligned CIR Amplitude (All {self.port_count} Ports) - {actual_fps} FPS')
        
        # Get latest frames
        frames = []
        valid_ports = []
        
        for i, fb in enumerate(self.frame_buffers):
            f = fb.latest()
            if f is not None:
                frames.append(f)
                valid_ports.append(i)
        
        if len(frames) == 0:
            return self.lines_cir + ([self.surf] if self.surf else [])
        
        if len(frames) < self.port_count:
            active_port_count = len(frames)
            active_steering = np.exp(1j * 2 * np.pi / LAMBDA * 0.04064 * 
                                    np.arange(active_port_count)[:, None] * np.sin(self.theta_rad))
        else:
            active_port_count = self.port_count
            active_steering = self.steering
        
        seqs = [f.seq for f in frames]
        if len(set(seqs)) != 1:
            return self.lines_cir + ([self.surf] if self.surf else [])
        
        # Process CIR data
        fixed_len = 6400
        cir_all = []
        lde_all = []
        
        for f in frames:
            cir_complex = np.array([complex(f.cir_real_imag[2*i], f.cir_real_imag[2*i+1]) 
                                   for i in range(100)])
            cir_upsampled = sp_signal.resample(cir_complex, len(cir_complex) * UPSAMPLE_FACTOR)
            
            if len(cir_upsampled) < fixed_len:
                cir_upsampled = np.concatenate([cir_upsampled, np.zeros(fixed_len - len(cir_upsampled))])
            else:
                cir_upsampled = cir_upsampled[:fixed_len]
            
            cir_all.append(cir_upsampled)
            lde_idx = round((f.fp_float - ORIGIN_START_IDX) * UPSAMPLE_FACTOR)
            lde_all.append(lde_idx)
        
        # Align CIR
        cir_mat = np.zeros((active_port_count, fixed_len), dtype=complex)
        ref_lde = lde_all[0]
        
        if ref_lde > (770 - 699) * 64 or ref_lde < (720 - 699) * 64:
            return self.lines_cir + ([self.surf] if self.surf else [])
        
        for m in range(active_port_count):
            cir_m = cir_all[m].copy()
            delay = lde_all[m] - ref_lde
            
            if delay > 0 and delay < len(cir_m):
                cir_m = np.concatenate([cir_m[delay:], np.zeros(delay)])
            elif delay < 0 and abs(delay) < len(cir_m):
                cir_m = np.concatenate([np.zeros(-delay), cir_m[:delay]])
            
            cir_mat[m, :] = cir_m
        
        # Normalize
        rx_pream_counts = np.array([f.rx_pream_count if f.rx_pream_count > 0 else 1 
                                    for f in frames])
        cir_mat = cir_mat / rx_pream_counts[:, None]
        
        # Update CIR plot
        x_vals = np.arange(fixed_len)
        for m in range(active_port_count):
            port_idx = valid_ports[m]
            aligned_mag = np.abs(cir_mat[m, :])
            self.lines_cir[port_idx].set_data(x_vals, aligned_mag)
        
        for m in range(self.port_count):
            if m not in valid_ports:
                self.lines_cir[m].set_data([], [])
        
        if frame_num % 10 == 0:
            self.ax_cir.set_xlim(0, fixed_len)
            if np.max(np.abs(cir_mat)) > 0:
                self.ax_cir.set_ylim(0, np.max(np.abs(cir_mat)) * 1.1)
        
        # Phase correction
        phi_ref = np.angle(cir_mat[:, ref_lde])
        rx = cir_mat * np.exp(-1j * phi_ref[:, None])
        #rx = rx / rx_pream_counts[:, None]
        
        # Read phase compensation vector
        try:
            csv_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'phase_comp_vector1.csv')
            best_c = read_complex_csv(csv_path)
            best_c = np.atleast_1d(best_c).ravel()
            
            # Align length (pad with 1 if too short, truncate if too long)
            if best_c.size < active_port_count:
                best_c = np.pad(best_c, (0, active_port_count - best_c.size), constant_values=1.0+0j)
            else:
                best_c = best_c[:active_port_count]
            
            # Apply to rx antenna by antenna
            rx = best_c.reshape(-1, 1) * rx
        except Exception as e:
            print(f"[Beamforming] Warning: Unable to load phase compensation vector: {e}")
            # If loading fails, continue using uncompensated rx
        
        # Range-Azimuth map - polar semicircle display
        valid_idx = ((np.arange(fixed_len) - ref_lde) / FS * C / 2) >= 0
        valid_idx = valid_idx & (((np.arange(fixed_len) - ref_lde) / FS * C / 2) <= self.max_range)
        
        if np.sum(valid_idx) == 0:
            return self.lines_cir + ([self.surf] if self.surf else [])
        
        rx_valid = rx[:, valid_idx]
        r_axis = ((np.arange(np.sum(valid_idx))) / FS * C / 2)
        
        # Calculate RA map
        RA_map = np.zeros((len(r_axis), len(self.theta_scan)))
        for i in range(len(r_axis)):
            x = rx_valid[:, i]
            RA_map[i, :] = np.abs(active_steering.T.conj() @ x) ** 2
        
        if np.max(RA_map) > 0:
            RA_map = RA_map / np.max(RA_map)
        
        # Range threshold
        r_thresh = 2
        RA_map[r_axis < r_thresh, :] = 0
        if np.max(RA_map) > 0:
            RA_map = RA_map / np.max(RA_map)
        
        # Prepare polar coordinate data
        theta_polar = np.deg2rad(self.theta_scan)
        THETA, R = np.meshgrid(theta_polar, r_axis)
        
        # First creation or data refresh - use pcolormesh instead of contourf (faster!)
        if self.surf is None:
            self.surf = self.ax_bf.pcolormesh(
                THETA, R, RA_map,
                cmap='jet',
                vmin=0,
                vmax=1,
                shading='auto'
            )
            self.colorbar = plt.colorbar(self.surf, ax=self.ax_bf, pad=0.1)
        else:
            # Directly update data (super fast!)
            self.surf.set_array(RA_map.ravel())
            if np.max(RA_map) > 0:
                self.surf.set_clim(0, 1)
        
        return self.lines_cir + [self.surf]

# ================== Range-Doppler Visualization ==================
class RangeDopplerVisualizer:
    """Range-Doppler visualization"""
    def __init__(self, frame_buffers, port_count):
        self.frame_buffers = frame_buffers
        self.port_count = port_count
        
        self.velocity_axis = ((np.arange(RD_WINDOW) - RD_WINDOW / 2) * 
                             (LAMBDA * FRAME_RATE / 2 / RD_WINDOW))
        
        self.fixed_max_len = WINDOW_LEFT + WINDOW_RIGHT + 1
        self.range_axis = ((np.arange(self.fixed_max_len) - WINDOW_LEFT) / FS * C / 2)
        
        rows = int(np.ceil(np.sqrt(port_count)))
        cols = int(np.ceil(port_count / rows))
        
        self.fig, self.axes = plt.subplots(rows, cols, figsize=(12, 8))
        self.fig.canvas.manager.set_window_title(f'Range-Doppler Maps ({port_count} Antennas)')
        
        if port_count == 1:
            self.axes = np.array([self.axes])
        self.axes = self.axes.flatten()
        
        self.images = []
        
        for m in range(port_count):
            self.axes[m].set_title(f'Antenna {m+1}')
            self.axes[m].set_xlabel('Velocity (m/s)')
            self.axes[m].set_ylabel('Range (m)')
            
            img = self.axes[m].imshow(
                np.zeros((self.fixed_max_len, RD_WINDOW)), 
                aspect='auto', 
                cmap='jet', 
                origin='lower',
                extent=[self.velocity_axis[0], self.velocity_axis[-1], 
                       self.range_axis[0], self.range_axis[-1]],
                vmin=0, vmax=1
            )
            self.images.append(img)
            plt.colorbar(img, ax=self.axes[m])
        
        for m in range(port_count, len(self.axes)):
            self.axes[m].axis('off')
    
    def update(self, frame_num):
        """Update visualization"""
        frames_by_port = []
        for fb in self.frame_buffers:
            frames = fb.get_last_n(RD_WINDOW)
            if len(frames) < RD_WINDOW:
                return self.images
            frames_by_port.append(frames)
        
        fixed_max_len = WINDOW_LEFT + WINDOW_RIGHT + 1
        
        for m in range(self.port_count):
            try:
                # ===== Step 1: Collect CIR from all frames and upsample =====
                fixed_len = 6400
                cir_all = []
                lde_all = []
                
                for frame in frames_by_port[m]:
                    cir_complex = np.array([complex(frame.cir_real_imag[2*k], 
                                                   frame.cir_real_imag[2*k+1]) 
                                           for k in range(100)])
                    cir_upsampled = sp_signal.resample(cir_complex, 
                                                      len(cir_complex) * UPSAMPLE_FACTOR)
                    
                    # Extend to fixed length
                    if len(cir_upsampled) < fixed_len:
                        cir_upsampled = np.concatenate([cir_upsampled, 
                                                       np.zeros(fixed_len - len(cir_upsampled))])
                    else:
                        cir_upsampled = cir_upsampled[:fixed_len]
                    
                    cir_all.append(cir_upsampled)
                    lde_idx = round((frame.fp_float - ORIGIN_START_IDX) * UPSAMPLE_FACTOR)
                    lde_all.append(lde_idx)
                
                # ===== Step 2: LDE alignment =====
                cir_mat = np.zeros((RD_WINDOW, fixed_len), dtype=complex)
                ref_lde = lde_all[0]  # Use first frame as reference
                
                # Check if ref_lde is within reasonable range
                if ref_lde > (770 - 699) * 64 or ref_lde < (720 - 699) * 64:
                    # ref_lde is unreasonable, skip this antenna
                    continue
                
                for i in range(RD_WINDOW):
                    cir_i = cir_all[i].copy()
                    delay = lde_all[i] - ref_lde
                    
                    # Time delay alignment
                    if delay > 0 and delay < len(cir_i):
                        cir_i = np.concatenate([cir_i[delay:], np.zeros(delay)])
                    elif delay < 0 and abs(delay) < len(cir_i):
                        cir_i = np.concatenate([np.zeros(-delay), cir_i[:delay]])
                    
                    cir_mat[i, :] = cir_i
                
                # ===== Step 3: Normalize =====
                rx_pream_counts = np.array([f.rx_pream_count if f.rx_pream_count > 0 else 1 
                                           for f in frames_by_port[m]])
                cir_mat = cir_mat / rx_pream_counts[:, None]
                
                # ===== Step 4: First path phase compensation =====
                phi_ref = np.angle(cir_mat[:, ref_lde])
                cir_mat = cir_mat * np.exp(-1j * phi_ref[:, None])
                
                # ===== Step 5: best_c phase compensation (best_c[m] for single antenna) =====
                try:
                    csv_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), 
                                          'phase_comp_vector1.csv')
                    best_c = read_complex_csv(csv_path)
                    best_c = np.atleast_1d(best_c).ravel()
                    
                    # Get compensation coefficient for current antenna
                    if m < len(best_c):
                        c_m = best_c[m]
                        cir_mat = c_m * cir_mat  # Apply to all frames
                except Exception as e:
                    # If loading fails, continue using uncompensated data
                    pass
                
                # ===== Step 6: Extract window near first path =====
                fixed_max_len = WINDOW_LEFT + WINDOW_RIGHT + 1
                signal_matrix = np.zeros((fixed_max_len, RD_WINDOW), dtype=complex)
                
                for i in range(RD_WINDOW):
                    start_idx = max(0, ref_lde - WINDOW_LEFT)
                    end_idx = min(fixed_len, ref_lde + WINDOW_RIGHT)
                    signal_win = cir_mat[i, start_idx:end_idx]
                    
                    sig_len = min(len(signal_win), fixed_max_len)
                    signal_matrix[:sig_len, i] = signal_win[:sig_len]
                
                # ===== Step 7: Static clutter removal =====
                for col in range(RD_WINDOW):
                    window_start = max(0, col - WIN_RADIUS)
                    window_end = min(RD_WINDOW, col + WIN_RADIUS + 1)
                    
                    window_cols = list(range(window_start, window_end))
                    if col in window_cols:
                        window_cols.remove(col)
                    
                    if window_cols:
                        mean_signal = np.mean(signal_matrix[:, window_cols], axis=1)
                        signal_matrix[:, col] -= mean_signal
                
                # ===== Step 8: FFT to calculate RD map =====
                rd_map = np.fft.fftshift(np.fft.fft(signal_matrix, axis=1), axes=1)
                rd_map = np.abs(rd_map)
                
                self.images[m].set_data(rd_map)
                if np.max(rd_map) > 0:
                    self.images[m].set_clim(0, np.max(rd_map))
            
            except Exception as e:
                print(f"[RangeDoppler] Antenna {m+1} update error: {e}")
                continue
        
        return self.images

# ================== Main Program ==================
class MultiPortCIRLogger:
    """Multi-port CIR acquisition main program"""
    def __init__(self):
        self.port_count = PORT_COUNT
        self.com_ports = COM_PORTS[:PORT_COUNT]
        self.frame_buffers = [FrameBuffer(MAX_QUEUE_LEN) for _ in range(PORT_COUNT)]
        self.data_queue = queue.Queue()
        self.workers = []
        self.frame_counts = [0] * PORT_COUNT
        self.running = True
        
        # Save control (shared dictionary)
        self.save_control = {'saving': False}
        
        # Create control window
        self.create_control_window()
    
    def create_control_window(self):
        """Create control window"""
        import tkinter as tk
        from tkinter import ttk
        
        self.control_window = tk.Tk()
        self.control_window.title("Data Acquisition Control")
        self.control_window.geometry("300x150")
        
        # Status label
        self.status_label = ttk.Label(
            self.control_window, 
            text="Status: Not Saving", 
            font=("Arial", 14),
            foreground="red"
        )
        self.status_label.pack(pady=20)
        
        # Save button
        self.save_button = ttk.Button(
            self.control_window,
            text="Start Saving Data",
            command=self.toggle_saving,
            width=20
        )
        self.save_button.pack(pady=10)
        
        # Frame count statistics label
        self.stats_label = ttk.Label(
            self.control_window,
            text="Saved: 0 frames",
            font=("Arial", 10)
        )
        self.stats_label.pack(pady=10)
        
        self.control_window.protocol("WM_DELETE_WINDOW", self.on_closing)
    
    def toggle_saving(self):
        """Toggle save state"""
        self.save_control['saving'] = not self.save_control['saving']
        
        if self.save_control['saving']:
            self.status_label.config(text="Status: Saving", foreground="green")
            self.save_button.config(text="Stop Saving Data")
            print(f"\n[{datetime.now().strftime('%H:%M:%S')}] ✓ Started saving data to CSV")
        else:
            self.status_label.config(text="Status: Stopped", foreground="red")
            self.save_button.config(text="Start Saving Data")
            print(f"\n[{datetime.now().strftime('%H:%M:%S')}] ✗ Stopped saving data")
    
    def on_closing(self):
        """Close control window"""
        self.running = False
        self.control_window.destroy()
        plt.close('all')
    
    def start(self):
        """Start acquisition system"""
        print(f"========== Starting {self.port_count}-Port CIR Acquisition System ==========\n")
        
        # Verify serial ports
        print("Checking serial port connections...")
        import serial.tools.list_ports
        available_ports = [port.device for port in serial.tools.list_ports.comports()]
        print(f"Available ports: {available_ports}")
        
        for i, port in enumerate(self.com_ports):
            if port not in available_ports:
                print(f"⚠ Warning: {port} not found, but will still attempt to connect...")
        
        print()
        
        # Start serial acquisition threads
        for i in range(self.port_count):
            csv_file = f'./20251108/antenna_data_port{i+1}_{self.port_count}ports_concurrent_localization_aoa_accuracy_0.csv'
            worker = SerialWorker(i + 1, self.com_ports[i], 
                                 self.frame_buffers[i], 
                                 self.data_queue, csv_file,
                                 self.save_control)
            worker.start()
            self.workers.append(worker)
            time.sleep(0.1)
        
        print("\nWaiting for serial ports to stabilize... (2 seconds)")
        time.sleep(2)
        
        print("\nStarting visualization processes (true parallel execution)...")
        
        # Create shared queues for inter-process communication
        bf_queue = mp.Queue(maxsize=5)
        rd_queue = mp.Queue(maxsize=5)
        data_ready = mp.Event()
        
        # Start Beamforming visualization process
        bf_process = Process(
            target=beamforming_process,
            args=(bf_queue, self.port_count, data_ready),
            daemon=True
        )
        bf_process.start()
        print(f"  ✓ Beamforming process started (PID: {bf_process.pid})")
        
        # Start Range-Doppler visualization process  
        rd_process = Process(
            target=rangedoppler_process,
            args=(rd_queue, self.port_count, data_ready),
            daemon=True
        )
        rd_process.start()
        print(f"  ✓ RangeDoppler process started (PID: {rd_process.pid})")
        
        # Start monitoring thread
        monitor_thread = threading.Thread(target=self.monitor_status, daemon=True)
        monitor_thread.start()
        
        # Start control window update thread
        control_thread = threading.Thread(target=self.update_control_window, daemon=True)
        control_thread.start()
        
        # Start data preparation thread (prepare data for visualization processes)
        data_prep_thread = threading.Thread(
            target=self.prepare_visualization_data,
            args=(bf_queue, rd_queue),
            daemon=True
        )
        data_prep_thread.start()
        
        print("\nSystem started, visualization processes running independently!")
        print("  - Beamforming: Target 10 Hz")
        print("  - RangeDoppler: Target 1 Hz\n")
        
        # Main thread keeps Tkinter window running
        try:
            while self.running:
                self.control_window.update()
                time.sleep(0.1)
        except KeyboardInterrupt:
            print("\nInterrupt signal detected")
        
        # Cleanup
        self.stop()
        bf_process.terminate()
        rd_process.terminate()
        bf_process.join(timeout=1)
        rd_process.join(timeout=1)
    
    def prepare_visualization_data(self, bf_queue, rd_queue):
        """Prepare data and send to visualization processes"""
        print("[Data Preparation Thread] Started")
        
        last_bf_time = time.time()
        last_rd_time = time.time()
        
        while self.running:
            try:
                current_time = time.time()
                
                # Prepare Beamforming data (10Hz)
                if current_time - last_bf_time >= 0.1:
                    bf_data = self.prepare_beamforming_data()
                    if bf_data is not None:
                        try:
                            bf_queue.put_nowait(bf_data)
                        except:
                            pass  # Queue full, skip
                    last_bf_time = current_time
                
                # Prepare RangeDoppler data (1Hz)
                if current_time - last_rd_time >= 1.0:
                    rd_data = self.prepare_rangedoppler_data()
                    if rd_data is not None:
                        try:
                            rd_queue.put_nowait(rd_data)
                        except:
                            pass
                    last_rd_time = current_time
                
                time.sleep(0.01)
            except Exception as e:
                print(f"[Data Preparation Thread] Error: {e}")
                time.sleep(0.1)
    
    def prepare_beamforming_data(self):
        """Prepare data required for Beamforming"""
        try:
            frames = []
            valid_ports = []
            
            for i, fb in enumerate(self.frame_buffers):
                f = fb.latest()
                if f is not None:
                    frames.append(f)
                    valid_ports.append(i)
            
            if len(frames) == 0:
                return None
            
            # Process CIR data
            fixed_len = 6400
            cir_all = []
            lde_all = []
            
            for f in frames:
                cir_complex = np.array([complex(f.cir_real_imag[2*i], f.cir_real_imag[2*i+1]) 
                                       for i in range(100)])
                cir_upsampled = sp_signal.resample(cir_complex, len(cir_complex) * UPSAMPLE_FACTOR)
                
                if len(cir_upsampled) < fixed_len:
                    cir_upsampled = np.concatenate([cir_upsampled, np.zeros(fixed_len - len(cir_upsampled))])
                else:
                    cir_upsampled = cir_upsampled[:fixed_len]
                
                cir_all.append(cir_upsampled)
                lde_idx = round((f.fp_float - ORIGIN_START_IDX) * UPSAMPLE_FACTOR)
                lde_all.append(lde_idx)
            
            # Align CIR
            active_port_count = len(frames)
            cir_mat = np.zeros((active_port_count, fixed_len), dtype=complex)
            ref_lde = lde_all[0]
            
            if ref_lde > (770 - 699) * 64 or ref_lde < (720 - 699) * 64:
                return None
            
            for m in range(active_port_count):
                cir_m = cir_all[m].copy()
                delay = lde_all[m] - ref_lde
                
                if delay > 0 and delay < len(cir_m):
                    cir_m = np.concatenate([cir_m[delay:], np.zeros(delay)])
                elif delay < 0 and abs(delay) < len(cir_m):
                    cir_m = np.concatenate([np.zeros(-delay), cir_m[:delay]])
                
                cir_mat[m, :] = cir_m
            
            # Normalize
            rx_pream_counts = np.array([f.rx_pream_count if f.rx_pream_count > 0 else 1 
                                        for f in frames])
            cir_mat = cir_mat / rx_pream_counts[:, None]
            
            # Phase correction
            phi_ref = np.angle(cir_mat[:, ref_lde])
            rx = cir_mat * np.exp(-1j * phi_ref[:, None])
            #rx = rx / rx_pream_counts[:, None]

            csv_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'phase_comp_vector1.csv')
            best_c = read_complex_csv(csv_path)
            best_c = np.atleast_1d(best_c).ravel()

            # Align length (pad with 1 if too short, truncate if too long)
            if best_c.size < active_port_count:
                best_c = np.pad(best_c, (0, active_port_count - best_c.size), constant_values=1.0+0j)
            else:
                best_c = best_c[:active_port_count]

            # Apply to rx antenna by antenna
            rx = best_c.reshape(-1, 1) * rx
            
            # Steering matrix
            theta_scan = np.arange(90, -91, -1)
            theta_rad = np.deg2rad(theta_scan)
            active_steering = np.exp(1j * 2 * np.pi / LAMBDA * 0.04064 * 
                                    np.arange(active_port_count)[:, None] * np.sin(theta_rad))
            
            return {
                'frames': frames,
                'valid_ports': valid_ports,
                'cir_mat': cir_mat,
                'rx': rx,
                'active_steering': active_steering,
                'fixed_len': fixed_len,
                'ref_lde': ref_lde
            }
        except Exception as e:
            return None
    
    def prepare_rangedoppler_data(self):
        """Prepare data required for RangeDoppler (with LDE alignment and phase compensation)"""
        try:
            frames_by_port = []
            for fb in self.frame_buffers:
                frames = fb.get_last_n(RD_WINDOW)
                if len(frames) < RD_WINDOW:
                    return None
                frames_by_port.append(frames)
            
            fixed_len = 6400
            fixed_max_len = WINDOW_LEFT + WINDOW_RIGHT + 1
            rd_maps = []
            
            # Read best_c compensation vector
            best_c = None
            try:
                csv_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), 
                                      'phase_comp_vector1.csv')
                best_c = read_complex_csv(csv_path)
                best_c = np.atleast_1d(best_c).ravel()
            except:
                pass
            
            for m in range(self.port_count):
                # ===== Step 1: Collect CIR from all frames and upsample =====
                cir_all = []
                lde_all = []
                
                for frame in frames_by_port[m]:
                    cir_complex = np.array([complex(frame.cir_real_imag[2*k], 
                                                   frame.cir_real_imag[2*k+1]) 
                                           for k in range(100)])
                    cir_upsampled = sp_signal.resample(cir_complex, 
                                                      len(cir_complex) * UPSAMPLE_FACTOR)
                    
                    # Extend to fixed length
                    if len(cir_upsampled) < fixed_len:
                        cir_upsampled = np.concatenate([cir_upsampled, 
                                                       np.zeros(fixed_len - len(cir_upsampled))])
                    else:
                        cir_upsampled = cir_upsampled[:fixed_len]
                    
                    cir_all.append(cir_upsampled)
                    lde_idx = round((frame.fp_float - ORIGIN_START_IDX) * UPSAMPLE_FACTOR)
                    lde_all.append(lde_idx)
                
                # ===== Step 2: LDE alignment =====
                cir_mat = np.zeros((RD_WINDOW, fixed_len), dtype=complex)
                ref_lde = lde_all[0]
                
                if ref_lde > (770 - 699) * 64 or ref_lde < (720 - 699) * 64:
                    rd_maps.append(np.zeros((fixed_max_len, RD_WINDOW)))
                    continue
                
                for i in range(RD_WINDOW):
                    cir_i = cir_all[i].copy()
                    delay = lde_all[i] - ref_lde
                    
                    if delay > 0 and delay < len(cir_i):
                        cir_i = np.concatenate([cir_i[delay:], np.zeros(delay)])
                    elif delay < 0 and abs(delay) < len(cir_i):
                        cir_i = np.concatenate([np.zeros(-delay), cir_i[:delay]])
                    
                    cir_mat[i, :] = cir_i
                
                # ===== Step 3: Normalize =====
                rx_pream_counts = np.array([f.rx_pream_count if f.rx_pream_count > 0 else 1 
                                           for f in frames_by_port[m]])
                cir_mat = cir_mat / rx_pream_counts[:, None]
                
                # ===== Step 4: First path phase compensation =====
                phi_ref = np.angle(cir_mat[:, ref_lde])
                cir_mat = cir_mat * np.exp(-1j * phi_ref[:, None])
                
                # ===== Step 5: best_c phase compensation =====
                if best_c is not None and m < len(best_c):
                    cir_mat = best_c[m] * cir_mat
                
                # ===== Step 6: Extract window near first path =====
                signal_matrix = np.zeros((fixed_max_len, RD_WINDOW), dtype=complex)
                
                for i in range(RD_WINDOW):
                    start_idx = max(0, ref_lde - WINDOW_LEFT)
                    end_idx = min(fixed_len, ref_lde + WINDOW_RIGHT)
                    signal_win = cir_mat[i, start_idx:end_idx]
                    
                    sig_len = min(len(signal_win), fixed_max_len)
                    signal_matrix[:sig_len, i] = signal_win[:sig_len]
                
                # ===== Step 7: Static clutter removal =====
                for col in range(RD_WINDOW):
                    window_start = max(0, col - WIN_RADIUS)
                    window_end = min(RD_WINDOW, col + WIN_RADIUS + 1)
                    
                    window_cols = list(range(window_start, window_end))
                    if col in window_cols:
                        window_cols.remove(col)
                    
                    if window_cols:
                        mean_signal = np.mean(signal_matrix[:, window_cols], axis=1)
                        signal_matrix[:, col] -= mean_signal
                
                # ===== Step 8: FFT to calculate RD map =====
                rd_map = np.fft.fftshift(np.fft.fft(signal_matrix, axis=1), axes=1)
                rd_map = np.abs(rd_map)
                rd_maps.append(rd_map)
            
            return rd_maps
        except Exception as e:
            return None

    def update_control_window(self):
        """Update control window"""
        while self.running:
            try:
                self.control_window.update()
                
                # Update statistics
                if self.save_control['saving']:
                    total_saved = sum(self.frame_counts)
                    self.stats_label.config(text=f"Received: {total_saved} frames")
                
                time.sleep(0.1)
            except:
                break
    
    def monitor_status(self):
        """Monitor acquisition status"""
        last_counts = [0] * self.port_count
        
        while self.running:
            time.sleep(1)
            
            # Read frame counts from queue
            while not self.data_queue.empty():
                try:
                    port_idx, _ = self.data_queue.get_nowait()
                    self.frame_counts[port_idx - 1] += 1
                except queue.Empty:
                    break
            
            # Calculate frame rate
            fps = []
            for i in range(self.port_count):
                diff = self.frame_counts[i] - last_counts[i]
                fps.append(diff)
                last_counts[i] = self.frame_counts[i]
            
            # Print status
            timestamp = datetime.now().strftime('%H:%M:%S')
            fps_str = ' '.join([f'P{i+1}:{fps[i]:.1f}' for i in range(self.port_count)])
            avg_fps = np.mean(fps)
            print(f"[{timestamp}] Real-time frame rate: {fps_str} (average: {avg_fps:.1f} fps)")
    
    def stop(self):
        """Stop acquisition"""
        print("\nStopping acquisition...")
        self.running = False
        
        for worker in self.workers:
            worker.stop()
        
        for worker in self.workers:
            worker.join(timeout=2)
        
        # Display statistics
        print("\n========== Acquisition Statistics ==========")
        for i in range(self.port_count):
            print(f"Port {i+1}: {self.frame_counts[i]} frames")
        print(f"Total frames: {sum(self.frame_counts)}")
        print("==========================================\n")

if __name__ == "__main__":
    # Set multiprocessing start method (required for Windows)
    mp.set_start_method('spawn', force=True)
    
    logger = MultiPortCIRLogger()
    try:
        logger.start()
    except KeyboardInterrupt:
        print("\nInterrupt signal detected")
        logger.stop()