import numpy as np
import serial
import collections
import csv
import threading
import signal
import time
import argparse
import re

try:
    import matplotlib.pyplot as plt
    import matplotlib.animation as animation
    import matplotlib.colors as mcolors
except ImportError:
    plt = None
    animation = None
    mcolors = None


CSI_MARKERS = ("+CSIDMA:", "CSI:")
RCP_CSI_FRAGMENT_RE = re.compile(r"csi report fragment len=(\d+) last=(\d+).*?(?: data=(.*))?$")


def extract_csi_payload(line):
    for marker in CSI_MARKERS:
        pos = line.find(marker)
        if pos >= 0:
            payload = line[pos + len(marker):].strip()
            payload = payload.replace("\\r", "").replace("\\n", "")
            return payload.strip("\"'")
    return None


def extract_rcp_csi_fragment(line):
    match = RCP_CSI_FRAGMENT_RE.search(line)
    if not match:
        return None

    fragment_len = int(match.group(1))
    last = int(match.group(2)) != 0
    data = match.group(3) or ""
    truncated = data.endswith("...") and fragment_len > len(data) - 3
    if truncated:
        data = data[:-3]
    return data, last, truncated


def subcarrier_indexes(nsc):
    if nsc == 52:
        return np.r_[np.arange(-26, 0), np.arange(1, 27)]
    return np.arange(nsc)


def read_csi_line(line):
    parts = [p.strip() for p in line.split(",") if p.strip() != ""]
    # parts = line.split(",")

    if len(parts) < 14:
        raise ValueError(f"incomplete CSI payload: got {len(parts)} fields, need at least 14")

    def is_converatable_to_int(value):
        is_decimal = False
        is_hexadecimal = False
        # Check if the string is convertible to a decimal integer
        try:
            int(value)
            is_decimal = True
        except ValueError:
            is_decimal = False

        # Check if the string is convertible to a hexadecimal integer
        try:
            int(value, 16)
            is_hexadecimal = True
        except ValueError:
            is_hexadecimal = False

        return is_decimal, is_hexadecimal
    
    # Parse the individual fields
    # wr timestamp print, fix in next version
    is_decimal, is_hexadecimal = is_converatable_to_int(parts[0])
    if is_decimal:
        timestamp = int(parts[0])
    elif is_hexadecimal:
        timestamp = int(parts[0], 16)
    else:
        timestamp = 0
        parts = ['0'] + parts
        
    csi_port = int(parts[1])
    mac_addr = parts[2].strip()
    rssi_dbm = int(parts[3])
    noise_dbm = int(parts[4])
    cfo_ppm = int(parts[5])
    agc = int(parts[6])
    ppdu_format = int(parts[7])
    rate = int(parts[8])
    mcs = int(parts[9])
    beamformed = int(parts[10])
    smoothing = int(parts[11])
    nsts = int(parts[12])
    nsc = int(parts[13])
    expected_parts = 14 + (nsts + 1) * nsc * 2
    if len(parts) < expected_parts:
        raise ValueError(f"incomplete CSI payload: got {len(parts)} fields, need {expected_parts}")
    
    csi_matrix = np.zeros((nsts+1, nsc), dtype=complex)
    index = 14
    for ists in range(nsts + 1):
        for isc in range(nsc):
            csi_matrix[ists,isc] = int(parts[index]) + 1j*int(parts[index + 1])
            index += 2
    
    return {
        "timestamp": timestamp,
        "csi_port": csi_port,
        "mac_addr": mac_addr,
        "rssi_dbm": rssi_dbm,
        "noise_dbm": noise_dbm,
        "cfo_ppm": cfo_ppm,
        "agc": agc,
        "ppdu_format": ppdu_format,
        "rate": rate,
        "mcs": mcs,
        "beamformed": beamformed,
        "smoothing": smoothing,
        "nsts": nsts,
        "nsc": nsc,
        "csi_matrix": csi_matrix
    }


class CSIData:
    def __init__(self, uart_port, uart_baudrate=2000000, csv_filename='csi_output.csv', csv_output=True,
                 plot=True, simulate=False, start_commands=None, skip_zero=True,
                 filter_format=None, filter_rate=None, filter_mcs=None,
                 filter_nsts=None, filter_nsc=None):
        self.uart_port = uart_port
        self.uart_baudrate = uart_baudrate
        self.csv_filename = csv_filename
        self.csv_output = csv_output
        self.csv_file = None
        self.plot = plot
        self.simulate = simulate
        self.start_commands = start_commands or []
        self.rcp_csi_fragments = []
        self.rcp_fragment_warning_printed = False
        self.invalid_csi_warning_printed = False
        self.skip_zero = skip_zero
        self.filter_format = filter_format
        self.filter_rate = filter_rate
        self.filter_mcs = filter_mcs
        self.filter_nsts = filter_nsts
        self.filter_nsc = filter_nsc
        self.filter_notice_printed = set()

        self.num_subcarriers = 52
        self.num_captures = 100
        self.csi_vector = None
        self.csi_matrix = np.zeros((self.num_subcarriers, self.num_captures), dtype=np.complex64)
        self.fifo = collections.deque(maxlen=self.num_captures)
        self.fifo_readline = collections.deque(maxlen=1000)
        
        self.running = False
        self.read_thread = None
        self.csv_thread = None
        self.plot_thread = None
        self.lock = threading.Lock()
    
    def parse_csi_line(self, line):
        return read_csi_line(line)

    def filter_reason(self, csi_data, csi_vector):
        if self.skip_zero and np.all(csi_vector == 0):
            return "all-zero CSI"
        filters = (
            ("format", "ppdu_format", self.filter_format),
            ("rate", "rate", self.filter_rate),
            ("mcs", "mcs", self.filter_mcs),
            ("nsts", "nsts", self.filter_nsts),
            ("nsc", "nsc", self.filter_nsc),
        )
        for name, key, expected in filters:
            if expected is not None and csi_data[key] != expected:
                return f"{name}={csi_data[key]} (want {expected})"
        return None

    def print_filter_notice(self, reason):
        if reason not in self.filter_notice_printed:
            print(f"Skip CSI frame: {reason}")
            self.filter_notice_printed.add(reason)

    def extract_payload(self, line):
        csi_payload = extract_csi_payload(line)
        if csi_payload:
            return csi_payload

        fragment = extract_rcp_csi_fragment(line)
        if fragment is None:
            return None

        data, last, truncated = fragment
        if truncated:
            self.rcp_csi_fragments = []
            if not self.rcp_fragment_warning_printed:
                print("Warning: RCP host CSI preview is truncated; this report cannot be parsed.")
                self.rcp_fragment_warning_printed = True
            return None

        if data:
            self.rcp_csi_fragments.append(data)

        if last:
            csi_payload = ''.join(self.rcp_csi_fragments)
            self.rcp_csi_fragments = []
            return csi_payload

        return None

    def read_uart(self):
        ser = serial.Serial(self.uart_port, self.uart_baudrate, timeout=1)
        for cmd in self.start_commands:
            print(f">> {cmd}")
            ser.write((cmd + "\r\n").encode("utf-8"))
            ser.flush()
            time.sleep(0.2)
        if self.csv_output:
            self.csv_file = open(self.csv_filename,'w')

        self.running = True
        while self.running:
            try:
                line = ser.readline()
                line = line.decode('utf-8').strip()
                print(line)
                csi_payload = self.extract_payload(line)
                if csi_payload:

                    try:
                        csi_data = self.parse_csi_line(csi_payload)
                    except ValueError as e:
                        if not self.invalid_csi_warning_printed:
                            print(f"Skip invalid CSI payload: {e}")
                            self.invalid_csi_warning_printed = True
                        continue
                    csi_vector = csi_data['csi_matrix'][0]
                    reason = self.filter_reason(csi_data, csi_vector)
                    if reason:
                        self.print_filter_notice(reason)
                        continue
                    if self.csv_file:
                        self.csv_file.write(csi_payload+'\n')
                        self.csv_file.flush()
                    print(csi_vector)
                    with self.lock:
                        self.csi_vector = csi_vector
                        self.update_csi_matrix(csi_vector)
                        self.fifo.append(csi_vector)
                        self.fifo_readline.append(csi_payload)
            except Exception as e:
                print(f"Error reading UART: {e}")
    
    def simulate_data(self):
        self.running = True
        while self.running:
            try:
                # Generate simulated CSI data
                csi_vector = np.random.randn(self.num_subcarriers) + 1j * np.random.randn(self.num_subcarriers)
                self.csi_vector = csi_vector
                self.update_csi_matrix(csi_vector)
                self.fifo.append(csi_vector)
                self.fifo_readline.append(','.join([f'{v.real},{v.imag}' for v in csi_vector]))
                time.sleep(0.1)  # Simulate a delay between readings
            except Exception as e:
                print(f"Error in data simulation: {e}")
                break
            
    # def update_csv(self):
    #      with open(self.csv_filename, 'w', newline='') as csvfile:
    #         # csvwriter = csv.writer(csvfile)
    #         while self.running:
    #             with self.lock:
    #                 if self.fifo_readline:
    #                     csi_line = self.fifo_readline.popleft()
    #                     # csvwriter.writerow(csi_line)
    #                     print(f'>> Write {csi_line}')
    #                     csvfile.write(csi_line + '\n')
    #                     csvfile.flush()  # Ensure data is written to the file
    
    def update_csi_matrix(self, csi_vector):
        if len(csi_vector) != self.num_subcarriers:
            self.num_subcarriers = len(csi_vector)
            self.csi_matrix = np.zeros((self.num_subcarriers, self.num_captures), dtype=np.complex64)
        # Shift the columns to the left
        self.csi_matrix = np.roll(self.csi_matrix, -1, axis=1)
        # Update the last column with the new csi_vector
        self.csi_matrix[:, -1] = csi_vector

    def update_plot_matrix(self):
        if plt is None:
            raise RuntimeError("matplotlib is required for --plot matrix")
        fig, (ax, ax1) = plt.subplots(nrows=2,ncols=1)
        norm = mcolors.LogNorm(vmin=1e1, vmax=1e2)
        cax = ax.matshow(20*np.log10(np.abs(self.csi_matrix) + 1e-4), cmap='plasma', norm=norm)
        # fig.colorbar(cax)
        cax1 = ax1.matshow(np.angle(self.csi_matrix)/np.pi*180, cmap='plasma', vmin=-180,vmax=180)

        ax.set_title("CSI Data")
        ax.set_xlabel("Frame No.")
        ax.set_ylabel("Subcarrier Index")
        if self.num_subcarriers == 52:
            ticks = [0, 12, 25, 26, 39, 51]
            labels = [-26, -14, -1, 1, 14, 26]
            ax.set_yticks(ticks)
            ax.set_yticklabels(labels)
            ax1.set_yticks(ticks)
            ax1.set_yticklabels(labels)

        def update_plot_inner(*args):
            with self.lock:
                cax.set_data(20 * np.log10(np.abs(self.csi_matrix) + 1e-4))
                cax1.set_data(np.angle(self.csi_matrix)/np.pi*180)
            return cax, cax1,

        ani = animation.FuncAnimation(fig, update_plot_inner, interval=100, blit=True, cache_frame_data=False)
        plt.show()
        
    def update_plot_line(self):
        if plt is None:
            raise RuntimeError("matplotlib is required for --plot line")
        fig, (ax, ax1, ax2) = plt.subplots(nrows=3, ncols=1, figsize=(5,12))
        line, = ax.plot([], [], 'r-')
        line1, = ax1.plot([], [], 'b.-')
        line2, = ax2.plot([], [], 'b-')
        x = subcarrier_indexes(self.num_subcarriers)
        ax.set_xlim(x[0], x[-1])
        ax.set_ylim(50, 100)
        ax.set_title("CSI Data")
        ax.set_xlabel("Subcarrier")
        ax.set_ylabel("Magnitude")
        
        ax1.set_xlim(-10000, 10000)
        ax1.set_ylim(-10000, 10000)
        
        ax2.set_xlim(x[0], x[-1])
        ax2.set_ylim(-180*3, 180*3)
        
        
        def update_plot_inner(*args):
            with self.lock:
                if self.csi_vector is not None:
                    x = subcarrier_indexes(self.num_subcarriers)
                    ax.set_xlim(x[0], x[-1])
                    ax2.set_xlim(x[0], x[-1])
                    y = 20*np.log10(np.abs(self.csi_vector) + 1e-4)
                    # z = np.angle(np.exp(1j*2*np.angle(self.csi_vector)))
                    # z = unwrap_phase(z,d=1)
                    # z = unwrap_phase(np.angle(self.csi_vector),d=4)
                    z = np.unwrap(np.angle(self.csi_vector))/np.pi*180
                    line.set_data(x, y)
                    line1.set_data(self.csi_vector.real, self.csi_vector.imag)
                    line2.set_data(x, z)
                    
            return line, line1, line2, 

        ani = animation.FuncAnimation(fig, update_plot_inner, interval=100, blit=True, cache_frame_data=False)
        plt.show()
        
    def run(self):
        def signal_handler(sig, frame):
            print('Stopping...')
            self.running = False
            if self.read_thread is not None:
                self.read_thread.join()
            if self.csv_thread is not None:
                self.csv_thread.join()
            if self.csv_file:
                self.csv_file.close()
            print('Stopped')

        signal.signal(signal.SIGINT, signal_handler)

        if self.plot in ('line', 'matrix') and plt is None:
            raise RuntimeError(f"matplotlib is required for --plot {self.plot}")

        if self.simulate:
            print("Simulation on.")
            self.read_thread = threading.Thread(target=self.simulate_data)
        else:
            print("Start UART reading thread...")
            self.read_thread = threading.Thread(target=self.read_uart)
        self.read_thread.start()

        # if self.csv_output:
        #     self.csv_thread = threading.Thread(target=self.update_csv)
        #     self.csv_thread.start()

        if self.plot == 'off':
            print("Plotting is turned off.")
        elif self.plot == 'line':
            print("Plotting the latest CSI capture as a line plot.")
            self.update_plot_line()
        elif self.plot == 'matrix':
            print("Plotting the CSI capture during the past several seconds as a matrix plot.")
            self.update_plot_matrix()

        signal.pause()
        
        if self.read_thread is not None:
            self.read_thread.join()
        print('Stopped')


def apply_mode_defaults(args):
    if args.mode == 'hostless':
        if args.uart_port is None:
            args.uart_port = '/dev/ttyACM4'
        if args.uart_baudrate is None:
            args.uart_baudrate = 2000000
    elif args.mode in ('ncp', 'rcp'):
        if args.uart_port is None:
            args.uart_port = '/dev/ttyACM5'
        if args.uart_baudrate is None:
            args.uart_baudrate = 921600
    else:
        if args.uart_port is None:
            args.uart_port = 'COM23'
        if args.uart_baudrate is None:
            args.uart_baudrate = 2000000


def freq_to_channel(freq):
    if freq == 2484:
        return 14
    if 2412 <= freq <= 2472 and (freq - 2412) % 5 == 0:
        return (freq - 2407) // 5
    raise ValueError(f"unsupported 2.4GHz frequency: {freq}")


def channel_to_freq(channel):
    if channel == 14:
        return 2484
    if 1 <= channel <= 13:
        return 2407 + channel * 5
    return channel


def channel_to_number(channel):
    if 1 <= channel <= 14:
        return channel
    return freq_to_channel(channel)


def build_start_commands(args):
    if args.start == 'off' or args.sim == 'on':
        return []

    if args.mode == 'hostless':
        freq = channel_to_freq(args.channel)
        if args.hostless_start == 'at':
            channel = channel_to_number(args.channel)
            return [
                f'AT+CSIDMASTART="{args.mac}",{args.buf},{args.csi_mode},{args.ts},{args.interval},{args.ready}',
                f"AT+CWMONITOR=1,{channel},,,0",
            ]
        return [
            f"csidma_start -a {args.mac} -b {args.buf} -m {args.csi_mode} -t {args.ts} -i {args.interval} -r {args.ready}",
            f"wifi_sniffer_on -i {args.sniffer_itf} -f {freq}",
        ]

    if args.mode == 'ncp':
        channel = channel_to_number(args.channel)
        return [
            f'AT+CSIDMASTART="{args.mac}",{args.buf},{args.csi_mode},{args.ts},{args.interval},{args.ready}',
            f"AT+CWMONITOR=1,{channel},,,0",
        ]

    if args.mode == 'rcp':
        channel = channel_to_number(args.channel)
        # WIFI_CSI_START already runs CSIDMASTART + CWMONITOR on the host side.
        return [
            f"WIFI_CSI_START {args.mac} {channel} {args.buf} {args.csi_mode} {args.ts} {args.interval} {args.ready}",
        ]

    return []


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='CSI Tool.')

    parser.add_argument('--mode', type=str, choices=['auto', 'hostless', 'ncp', 'rcp'], default='auto',
                        help="Realtime source mode. hostless reads the device console; ncp/rcp read the host console.")
    parser.add_argument('--uart_port', type=str, default=None,
                        help="UART port. Defaults: hostless=/dev/ttyACM4, ncp/rcp=/dev/ttyACM5.")
    parser.add_argument('--uart_baudrate', type=int, default=None,
                        help="UART baud rate. Defaults: hostless=2000000, ncp/rcp=921600.")
    parser.add_argument('--channel', type=int, default=2462,
                        help="WiFi channel number or frequency in MHz (default: 2462, channel 11).")
    parser.add_argument('--mac', type=str, default='A4:A9:30:84:6A:F2',
                        help="CSI MAC address (default: 'A4:A9:30:84:6A:F2')")
    parser.add_argument('--buf', type=int, default=0,
                        help="CSI buffer index (default: 0).")
    parser.add_argument('--csi_mode', type=int, default=0,
                        help="CSI mode: 0 legacy LTF, 1 HT LTF, 2 auto (default: 0).")
    parser.add_argument('--ts', type=int, default=2,
                        help="CSI timestamp mode: 0 off, 1 one-shot, 2 periodic, 3 FTM (default: 2).")
    parser.add_argument('--interval', type=int, default=100000,
                        help="CSI report interval (default: 100000)")
    parser.add_argument('--ready', type=int, default=1,
                        help="CSI ready-after-ISR policy: 0 pending, 1 always ready (default: 1).")
    parser.add_argument('--start', type=str, choices=['off', 'on'], default='on',
                        help="Send CSI start commands before reading UART (default: on).")
    parser.add_argument('--sniffer_itf', type=str, default='wl1',
                        help="Hostless sniffer interface (default: wl1).")
    parser.add_argument('--hostless_start', type=str, choices=['cli', 'at'], default='cli',
                        help="Hostless startup command style: cli or at (default: cli).")
    parser.add_argument('--csv_filename', type=str, default=None,
                        help="CSV filename for output (default: None). If not provided, no CSV output is generated.")
    parser.add_argument('--skip_zero', type=str, choices=['off', 'on'], default='on',
                        help="Skip CSI frames whose matrix is all zero (default: on).")
    parser.add_argument('--filter_format', type=int, default=None,
                        help="Only keep CSI frames with this PPDU format.")
    parser.add_argument('--filter_rate', type=int, default=None,
                        help="Only keep CSI frames with this legacy rate.")
    parser.add_argument('--filter_mcs', type=int, default=None,
                        help="Only keep CSI frames with this MCS.")
    parser.add_argument('--filter_nsts', type=int, default=None,
                        help="Only keep CSI frames with this NSTS value.")
    parser.add_argument('--filter_nsc', type=int, default=None,
                        help="Only keep CSI frames with this NSC value.")
    parser.add_argument('--plot', type=str, choices=['off', 'line', 'matrix'], default='off',
                        help="Plotting option (default: 'off'). Options are 'off', 'line', and 'matrix'.")
    parser.add_argument('--sim', type=str, choices=['off', 'on'], default='off',
                        help="")
    args = parser.parse_args()

    apply_mode_defaults(args)
    start_commands = build_start_commands(args)

    csidata = CSIData(uart_port=args.uart_port, uart_baudrate=args.uart_baudrate,
                      csv_filename=args.csv_filename,
                      csv_output=False if args.csv_filename is None else True,
                      plot=args.plot, simulate=args.sim=='on',
                      start_commands=start_commands,
                      skip_zero=args.skip_zero=='on',
                      filter_format=args.filter_format,
                      filter_rate=args.filter_rate,
                      filter_mcs=args.filter_mcs,
                      filter_nsts=args.filter_nsts,
                      filter_nsc=args.filter_nsc)
    csidata.run()
