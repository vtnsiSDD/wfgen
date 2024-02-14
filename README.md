# WFGen

Waveform generator designed to create waveforms in file or over the air format.

This toolkit is under development and no warranty is expressed or implied with.
USE AT OWN RISK.

It is assumed that this repo has been downloaded and can be installed with git.
The top level folder of this repo is assumed to be `wfgen`

This toolkit relies on a trusted transmitter approach.
- The computer hosting the `server` interface must have a trusted timestamp
    - An NTP server should provide sufficient reliability
- The receiver is user defined and controlled
    - Any time differential should be handled at the receiver
    - Any frequency differential can be interpretted as transceiver imperfection

Computers connected to transmit devices host a `server` and the single
user connects with the `client`.

All development has relied on the B2XX series USRPs.

## Author

- Initial worker on applications and supporting code: Joseph Gaeddert
- Continued development and CLI: Bill Clark

Contact: bill.clark@vt.edu

## Install

This has largely been developed and tested on Ubuntu 20.04/22.04.

Some functionality is currently tied to python and GNU Radio, but are not required (hopefully).

GNU Radio is assumed to be installed with PyBombs, but if not, just make sure to export
the `PYBOMBS_PREFIX=/usr` (on Ubuntu) for functionality.

### Dependencies

- git
- automake
- autoconfg
- pthread
- fftw
- zmq
- czmq
- boost_system
- libyaml
- pip

#### Submodule Dependency

- liquid-dsp

### Initial steps

Assuming tools and libraries are installed and available

For development ease, the repo expects to be wrapped into a virtual environment

```bash
virtualenv wfgen-dev
```

Then the following needs to be appended to the `activate` script for linking
Adjust/comment out the `/mnt/ramdisk` creation as desired.

Alternatively, the script at `scripts/wfgen_env.sh` can be used in place of
altering the activate script. This will try to load what is needed into the
environment and will set `WFGEN_ENV=1` if fully successful.

```bash
if [[ $LD_LIBRARY_PATH != *${VIRTUAL_ENV}* ]]; then
    export LD_LIBRARY_PATH="${VIRTUAL_ENV}/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
fi
if [[ $LIBRARY_PATH != *${VIRTUAL_ENV}* ]]; then
    export LIBRARY_PATH="${VIRTUAL_ENV}/lib${LIBRARY_PATH:+:$LIBRARY_PATH}"
fi
if [ ! -z ${AUDIO_FOLDER+x} ]; then
    RAMDISK_SIZE="4g" ## change as desired
    if [ ! -d "/mnt/ramdisk/$(basename $AUDIO_FOLDER)" ]; then
        if [ ! -d "/mnt/ramdisk" ]; then
            sudo mkdir /mnt/ramdisk
        fi
        sudo mount -t tmpfs -o size=${RAMDISK_SIZE} tmpgs /mnt/ramdisk
        cp -r $AUDIO_FOLDER /mnt/ramdisk/
    fi
    if [ -z ${WFGEN_AUDIO_FOLDER+x} ]; then
        export WFGEN_AUDIO_FOLDER="/mnt/ramdisk/$(basename $AUDIO_FOLDER)"
    fi
else
    echo "AUDIO_FOLDER isn't set, so analog waveforms won't work properly"
    echo " Need to set WFGEN_AUDIO_FOLDER manually"
fi
```

The current functionality of analog waveforms assumes there's an `AUDIO_FOLDER` that
contains wav files that can be used as an analog source. If not set before activating
the environment, manually set `WFGEN_AUDIO_FOLDER` for automatic audio selection.
It is recommended that the audio be on a temp filesystem in RAM for best performance.

Once the `VIRTUAL_ENV` is setup, the make files should be functional and will install
the submodule `liquid-dsp`, but if already installed just tweak the makefile to skip
this step.

The apps are then created in `build/apps` and if installed will go to `VIRTUAL_ENV/bin`

```bash
make -j4 install
```

There are logging programs `logger_server` and `logger_file` that are meant for debugging
and are not documented, use at your discretion.

The main waveform apps then start with `wfgen_` and have built in help commands
```bash
wfgen_linmod -h
```

# The CLI

The CLI relies on python module cli, and the module can be installed with pip. Though it can
be installed, the code execution still takes place from the root folder of the repo.

### Install (as tested)

```bash
(wfgen-dev) wfgen$ pip install -e .
```

The CLI operations in client/server configuration and can have multiple servers connected, but
multiple clients can result in unexpected behavior.

## Usage

### Server

The server is the computer system connected to the radio devices.
For better resource management, there can be as many servers as
provides enough data throughput to utilize all desired devices.

### Client

The client is the single user connection to all available servers.
The client has a CLI available for manual control, but is accessible
through a class object for further extension, and is used by the CLI
for an example usage.

### Example System Setup

```
Server 1:
X.X.X.5 : 50000
3x B210

Server 2:
X.X.X.8 : 50000
1x B205

Server 3:
X.X.X.20 : 50000
2x B200
```
 
```
Client:
# (assuming ports are open between client/servers)
client.py -conn X.X.X.5 50000 0 --conn X.X.X.8 50000 0 --conn X.X.X.20 50000 0 --verbose
|||||||
# (assuming only ssh is open between client/servers)
client.py -conn X.X.X.5 50000 1 --conn X.X.X.8 50000 1 --conn X.X.X.20 50000 1 --verbose

(Cmd) get_radios
[shows 6 radios available]
```

### Server Side

(Assuming installed)

```bash
python3 wfgen_server --help
usage: wfgen_server [-h] [--addr ADDR] [--port PORT] [--octo-addr OCTO_ADDR] [--octo-port OCTO_PORT]
                 [--uhd-args UHD_ARGS] [--log-server]

optional arguments:
  -h, --help            show this help message and exit
  --addr ADDR           Interface address to bind to. Defaults to internet connection.
  --port PORT           Port to bind to (def: 50000)
  --uhd-args UHD_ARGS   Limit to devices whose flag provided will find (def: all uhd devices)
  --log-server          Use if a log-server is active (meant for debugging)

python3 wfgen_server --addr 127.0.0.1
Starting server...started
```

At this point the server is up and bound to `tcp://127.0.0.1:50000` and will have the ability to see
all UHD USRP devices. To constrain to only specific radios `--uhd-args` can be used repeatedly with
any UHD arguments that help find the devices. For example, to limit to only the B2XX series devices

```bash
python3 wfgen_server --addr 127.0.0.1 --uhd-args type=b200
```

### Client Side (CLI)

The client can be started as a program to use the designed command line interface.

(Assuming installed)

```bash
python3 wfgen_client --help
usage: wfgen_client [-h] [--conn addr port use_ssh] [--verbose] [--dev] [--log-server]

optional arguments:
  -h, --help            show this help message and exit
  --conn addr port use_ssh
                        Unique interface address to connect to. Default: host<internet connection IP> 50000 False
  --verbose             Should the cli spit out everything?
  --dev                 Should the cli spit out dev messages?
  --log-server          Use if a log-server is active (meant for debugging)
```

To connect to the server above

```bash
python3 wfgen_client --conn 127.0.0.1 50000 False --verbose

** -------------------------------------------------------------------------- **
** Command List
** - help
**     -- Show this help menu
** - exit
**     -- Closes the cli
** - shutdown
**     -- Closes both the cli and server
** - get_radios
**     -- The process of how the cli figures out radio numbers
** - get_active
**     -- List of tuples -- each tuple is an active process on the server
** - get_finished
**     -- List of tuples -- each tuple is an finished process on the server
** - get_truth local_filename.json
**     -- Saves all sent signals since start/last call into local_filename.json
** - start_radio <device number> <profile type> <profile name> [options]
**     start_radio 0 profile qpsk frequency 2.45e6
**     -- This starts device 0 (with need args) at the specified frequency
**     -- This start_radio command will be revised
** - run_random [options]
**     run_random radios 0,1 bands 2.4e9,2.5e9 duration_limits 2.0,5.0 \
**         bw_limits 50e3,1e6 profiles qpsk,fsk16
**     -- This starts device 0,1 (with need args) with carrier in [2.4e9,2.5e9] Hz
**        with signals of on time in range [2,5] seconds,
**        with energy bandwidth in range [50e3,1e6] Hz
**        with profiles qpsk and fsk16
**     -- This run_random command will be revised
** - run_script [seed int, ... ] <script.json filepath/ dir with main.json>
**     run_script 1 2 3 4 5 configs
**     run_script 1 2 3 4 5 configs/main.json
**     run_script 1 2 3 4 5 truth_scripts/output-1a-truth.json
**     -- This starts all devices queue by the script
**     -- This run_script command will be revised
** - kill <proc_id>
**     -- Send kill signal to subprocess running a profile
** -------------------------------------------------------------------------- **

(Cmd) 

```

The `--conn` flag can be used multiple times to connect to multiple servers at the same time.
The final component of the `--conn` flag is whether ssh should be used to connect instead of
direct port connection, and is intended for connecting to systems whose ports aren't necessarily
open due to security concerns, yet can still be connected to through ssh.
The first command will always need to be `get_radios` in order for any other functionality

```python
(Cmd) get_radios
Radio List (2):
     0 server=0   args='type=b200,serial=30875A1'
     1 server=0   args='type=b200,serial=30E6262'

(Cmd)
```

Manual control can then be used to start one radio at a time.

```python
(Cmd) start_radio 0 static bpsk frequency 2.45e9 gain 70 bw 0.7 rate 1e6
['frequency', '2.45e9', 'gain', '70', 'bw', '0.7', 'rate', '1e6']
frequency 2.45e9
gain 70
bw 0.7
rate 1e6
{'frequency': '2.45e9', 'gain': '70', 'bw': '0.7', 'rate': '1e6'}
src_cl: '0 static bpsk frequency 2.45e9 gain 70 bw 0.7 rate 1e6'
src_sl: ['0', 'static', 'bpsk', 'frequency', '2.45e9', 'gain', '70', 'bw', '0.7', 'rate', '1e6']
sending command: ['start_radio', 'static', '-a type=b200,serial=30875A1', 'bpsk', 'frequency', '2.45e9', 'gain', '70', 'bw', '0.7', 'rate', '1e6']
start_radio 0 static bpsk frequency 2.45e9 gain 70 bw 0.7 rate 1e6
reply: Starting process 1903712 /data/local/wfgen_reports/20240129082959_truth/truth_dev_30875A1_instance_00000.json
```

The process can then be ended with

```python
(Cmd) kill 1903712
sending command: ['kill', '1903712']
kill 1903712
reply: Killing process 1903712
```

On the server side that interaction should produce something like this:
```python
python3 wfgen_server --addr 127.0.0.1 --uhd-args type=b200
Starting server...started
['frequency', '2.45e9', 'gain', '70', 'bw', '0.7', 'rate', '1e6', 'json', '/data/local/wfgen_reports/20240129082959_truth/truth_dev_30875A1_instance_00000.json']
frequency 2.45e9
gain 70
bw 0.7
rate 1e6
json /data/local/wfgen_reports/20240129082959_truth/truth_dev_30875A1_instance_00000.json
{'frequency': '2.45e9', 'gain': '70', 'bw': '0.7', 'rate': '1e6', 'json': '/data/local/wfgen_reports/20240129082959_truth/truth_dev_30875A1_instance_00000.json'}
Making a bpsk profile
RUNNING WITH THIS:
wfgen_linmod -a type=b200,serial=30875A1 -b 0.7 -f 2.45e9 -g 70 -r 1e6 -j /data/local/wfgen_reports/20240129082959_truth/truth_dev_30875A1_instance_00000.json -M bpsk
Using:
  modulation:   bpsk
  freq:         2450000000.000
  rate:         1000000.000
  bw:           0.700
  gain:         70.000
  args:         type=b200,serial=30875A1
  grange:       0.000
  cycle:        2.000
[INFO] [UHD] linux; GNU C++ version 9.4.0; Boost_107100; UHD_4.4.0.SCISRS_ECTB_BRANCH-0-g5fac246b
[INFO] [B200] Detected Device: B210
[INFO] [B200] Loading FPGA image: /opt/gnuradio/v3.10/share/uhd/images/usrp_b210_fpga.bin...
[INFO] [B200] Operating over USB 3.
[INFO] [B200] Detecting internal GPSDO....
[INFO] [GPS] No GPSDO found
[INFO] [B200] Initialize CODEC control...
[INFO] [B200] Initialize Radio control...
[INFO] [B200] Performing register loopback test...
[INFO] [B200] Register loopback test passed
[INFO] [B200] Performing register loopback test...
[INFO] [B200] Register loopback test passed
[INFO] [B200] Setting master clock rate selection to 'automatic'.
[INFO] [B200] Asking for clock rate 16.000000 MHz...
[INFO] [B200] Actually got clock rate 16.000000 MHz.
[INFO] [B200] Asking for clock rate 32.000000 MHz...
[INFO] [B200] Actually got clock rate 32.000000 MHz.
2000000,2500
running (hit CTRL-C to stop)
LINMOD ---> ctrl+c received --> exiting
usrp data transfer complete
Timestamp at program start: cpu sec: 1706535016.234323740
Connecting to radio at: cpu sec: 1706535016.234411716
Starting to send at: cpu sec: 1706535026.952406406
Stopping send at: cpu sec: 1706535032.096147776
Radio should be stopped at: cpu sec: 1706535032.159663200
.json log written to /data/local/wfgen_reports/20240129082959_truth/truth_dev_30875A1_instance_00000.json
```

The truth file containing information on everything that was transmitted can then be pulled to the client
with the command

```python
(Cmd) get_truth demo_run.json
'/tmp/tmpvfx_ct91/truth_report_000.json']
Tracking new source: type=b200,serial=30875A1
results written to demo_run.json
truth file: demo_run.json
```

-- Note: If the json is not proper, the report will not contain the improper json, but the information
on the server for the specified signal
(ex: /data/local/wfgen_reports/20240129082959_truth/truth_dev_30875A1_instance_00000.json)
will still contain useful information, but will require human intervention to correct.


The previous runs can then be rerun from the script in a stochastic approximation of the original.

```python
(Cmd) run_script demo_run.json
['/tmp/tmpc1jrvnd8/truth_report_000.json']
Tracking new source: type=b200,serial=30875A1
results written to demo_rerun.json
truth file: demo_rerun.json
```

In case just a random run is required with a limited set of signals, this can be achieved with
the `run_radom` command. With the truth recovered in the same way. An example is shown below.
Currently limitied to `static` profiles.

```python
(Cmd) run_random radios 0,1 duration_limits 20,60 bands 2.4e9,2.5e9 bw_limits 50e3,1e6 profiles qpsk
Starting random run: 940287
(Cmd) kill 940287sending command: ['kill', '940287']
kill 940287
reply: Killing process 940287
(Cmd) get_truth demo_random.json
['/tmp/tmphbkcexaa/truth_report_000.json']
Tracking new source: type=b200,serial=31F59F6
Tracking new source: type=b200,serial=3164B4D
results written to demo_random.json
truth file: demo_random.json
```

For more complex randomized runs, a scenario can be created.
This requires a bit more creation in order to successfully run, but an example scenario can be found
in `wfgen/scenarios/example_scenario_000.json` which runs a noise waveform for a set time and
parameter constraints in the directories within directories.

```python
(Cmd) run_script scenarios/example_scenario_000.json
Starting scripted run: 1015948
```

And likewise, the truth file can be pulled in the same fashion.


----------------------

Use tab in the CLI to find possible choices.

----------------------


## Known Bugs

- Using the up and down keys in the command line is possible, but going up and then down can result in the
CLI's pointer to get offset and make it harder to see exactly what is going on.

- When the command wraps around the edge of the window the command itself can dissapear. This is usually
fixed by hitting tab to refresh the line.

- Precision roundoff in re-running scripts

- Required directory `/data/local/wfgen_reports` is hardcoded and not created during install
