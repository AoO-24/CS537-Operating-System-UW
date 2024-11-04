#!/usr/bin/python3
import socket
import argparse
import subprocess
import os
import sys
import traceback



# Returns a list of n free ports
# There's no guarantee that these ports
# will still be free when the caller wants
# to use them
def find_free_ports(n):
    tmp_socks = []
    ports = []
    # Open n sockets, let the OS choose the port
    for i in range(n):
        tsock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        tsock.bind(('', 0))
        _, port = tsock.getsockname()
        tmp_socks.append(tsock)
        ports.append(port)
    
    # Clean
    for s in tmp_socks:
        s.close()
    return ports
    

# Call `make all` in `path`
# Path should contain a Makefile that compiles the project
def compile(path):
        try:
            result = subprocess.run(['make', 'all'], cwd=path, check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            if result.returncode == 0:
                return 0
            else:
                print("Make all failed with the following error:")
                print(result.stderr.decode('utf-8'))
                sys.exit(1)
        except subprocess.CalledProcessError as e:
            print("Make all failed with the following error:")
            print(e.stderr.decode('utf-8'))
            sys.exit(1)

# Run the target server on 'host:port'
# Currently we're using Python http server as our target server
def run_target(host, port, target_home):
    cmd = ['python3', '-m', 'http.server', f'{port}']
    return subprocess.Popen(cmd, cwd=target_home, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

# Run the proxy server
# ports: 'nl' number of free ports, listener threads listen on these ports
# nl: Number of listener threads - len(ports) should be equal to nl
# nw: Number of worker threads
# server_host: Host address of the target server
# server_port: Port number of the target server
# q: Maximum queue size
# test_home: Where the source files for the proxy server are located
def run_proxy(ports, nl, nw, server_host, server_port, q, test_home):
    compile(test_home)
    program_name = test_home + '/proxyserver'
    str_ports = [str(port) for port in ports]
    cmd = [program_name,
                       '-l', str(nl), *str_ports,
                       '-w', str(nw),
                       '-i', socket.gethostbyname(server_host),
                       '-p', str(server_port),
                       '-q', str(q)]
    return subprocess.Popen(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

# Calls run_proxy and run_target functions (if no_target is not set)
# Used when 'start.py' is called directly
def run(args):
    verbose = args.verbose
    num_listener = args.lnum
    num_worker = args.wnum
    server_host = args.host 
    server_port = args.port
    q = args.qsize
    no_target = args.notarget
    test_home = args.testhome
    target_home = args.targethome
    ports = []
    if args.port is not None:
        proxy_ports = find_free_ports(num_listener)
    else:
        ports = find_free_ports(num_listener + 1)
        proxy_ports = ports[:-1]
        server_port = ports[-1]
    
    print(f'Ports for listener threads: {proxy_ports}')
    print(f'Port for target server: {server_port}')
    proxy_process = None
    target_process = None
    try:
        if not no_target:
            target_process = run_target(server_host, server_port, target_home)
        proxy_process = run_proxy(proxy_ports, num_listener, num_worker, server_host, server_port, q, test_home)
    except:
        if target_process is not None:
            target_process.terminate()
        if proxy_process is not None:
            proxy_process.terminate()
        print(traceback.format_exc())

    return target_process, proxy_process


# Parse input arguments and call run
# Used when 'start.py' is called directly
def main():
    parser = argparse.ArgumentParser(description='A simple argument parser example')

    # Add arguments
    parser.add_argument('--host', type=str, default='localhost', help='Target host of the request')
    parser.add_argument('-l', '--lnum', type=int, default=1, help='Number of listener threads')
    parser.add_argument('-w', '--wnum', type=int, default=1, help='Number of worker threads')
    parser.add_argument('-p', '--port', type=int, help='Target port of the request')
    parser.add_argument('-q', '--qsize', default=10, type=int, help='Maximum size of the pq')
    parser.add_argument('-t', '--notarget', action='store_true',
                         help='If this is set, the target server is not launched.\
                            This should be used if the proxy is being tested with\
                                a server in the Internet or there is already a\
                                server running on the local machine')
    parser.add_argument('--testhome', type=str, default='./', help='Where the source files of the solution are located. There should also be a Makefile in this directory with at least the `all` target')
    parser.add_argument('--targethome', type=str, default='./public_html', help='The target server runs in this directory. File requests are resolved relative to this directory.')
    parser.add_argument('-v', '--verbose', action='store_true', help='Verbosity')

    # Parse the arguments
    args = parser.parse_args()

    run(args) 



if __name__ == '__main__':
    main()
