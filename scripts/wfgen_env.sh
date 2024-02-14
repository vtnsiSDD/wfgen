#!/usr/bin/env bash

### community wiki
# https://stackoverflow.com/questions/59895/how-do-i-get-the-directory-where-a-bash-script-is-located-from-within-the-script

SOURCE=${BASH_SOURCE[0]}
while [ -L "$SOURCE" ]; do
    DIR=$( cd -P "$( dirname "$SOURCE" )" >/dev/null 2>&1 && pwd )
    SOURCE=$( readlink "$SOURCE" )
    [[ $SOURCE != /* ]] && SOURCE=$DIR/$SOURCE
done
SCRIPT_DIR=$( cd -P "$( dirname "${SOURCE}" )" &> /dev/null && pwd)
SCRIPT_PAR=$( dirname "${SCRIPT_DIR}" )
unset SOURCE
unset DIR


echo "Setting up the wfgen environment"

if [ -z "${PYBOMBS_PREFIX+x}" ]; then
    if [ ! -f "${SCRIPT_PAR}/setup_env.sh" ]; then
        echo "PYBOMBS_PREFIX is not active."
        echo " searching for typical UHD install."
        if [ -f "/usr/include/uhd.h" ]; then
            echo "UHD is installed at /usr"
            export PYBOMBS_PREFIX=/usr
        elif [ -f "/usr/local/include/uhd.h" ]; then
            echo "UHD is installed at /usr/local"
            export PYBOMBS_PREFIX=/usr/local
        fi
        if [ -z "${PYBOMBS_PREFIX+x}" ]; then
            echo "Cannot find uhd.h, set PYBOMBS_PREFIX such that"
            echo "   \${PYBOMBS_PREFIX}/include/uhd.h exists"
            echo "  FAILURE"
            (return 0 2>/dev/null) && return 1 || exit 1
        fi
    else
        echo "Found likely PYBOMBS_PREFIX setup sript, activating."
        source "${SCRIPT_PAR}/setup_env.sh"

        if [[ $? == 0 ]]; then
            echo "  SUCCESS -- ${SCRIPT_PAR}/setup_env.sh sourced"
        else
            echo "Could not source the setup_env.sh script"
            echo "  FAILURE"
            (return 0 2>/dev/null) && return 1 || exit 1
        fi
        if [ -z "${PYBOMBS_PREFIX+x}" ]; then
            echo "PYBOMBS_PREFIX still is not set."
            echo "Manually set PYBOMBS_PREFIX such that"
            echo "   \${PYBOMBS_PREFIX}/include/uhd.h exists"
            echo "  FAILURE"
            (return 0 2>/dev/null) && return 1 || exit 1
        fi
        if [ ! -z ${VIRTUAL_ENV} ]; then
            echo "PYBOMBS environment script activated the virtual environment"
            echo "   ${VIRUTAL_ENV}"
        fi
    fi
fi

if [ -z "${VIRTUAL_ENV+x}" ]; then
    echo "Activating virtualenv located at: $( dirname "${SCRIPT_DIR}" )"
    source "${SCRIPT_DIR}/activate"

    if [[ $? != 0 ]]; then
        echo "Could not source the virtualenv"
        echo "  FAILURE"
        (return 0 2>/dev/null) && return 1 || exit 1
    fi
fi

if [[ $LD_LIBRARY_PATH != *${VIRTUAL_ENV}* ]]; then
    export LD_LIBRARY_PATH="${VIRTUAL_ENV}/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
fi
if [[ $LIBRARY_PATH != *${VIRTUAL_ENV}* ]]; then
    export LIBRARY_PATH="${VIRTUAL_ENV}/lib${LIBRARY_PATH:+:$LIBRARY_PATH}"
fi
if [ ! -z ${AUDIO_FOLDER+x} ]; then
    RAMDISK_SIZE="4g"
    if [ ! -d "/mnt/ramdisk/$(basename $AUDIO_FOLDER)" ]; then
        if [ ! -d "/mnt/ramdisk" ]; then
            sudo mkdir /mnt/ramdisk
        fi
        sudo mount -t tmpfs -o size=4g tmpgs /mnt/ramdisk
        cp -r $AUDIO_FOLDER /mnt/ramdisk/
    fi
    if [ -z "${WFGEN_AUDIO_FOLDER+x}" ]; then
        export WFGEN_AUDIO_FOLDER="/mnt/ramdisk/$(basename $AUDIO_FOLDER)"
    fi
else
    echo "AUDIO_FOLDER isn't set, so analog waveforms won't work properly"
    echo " Need to set WFGEN_AUDIO_FOLDER manually"
fi

start_wfgen_wav_server() {
    wav_server.py --bind-port 60000 1>/dev/null 2&>1 & disown
    export WFGEN_AUDIO_SERVER='tcp://127.0.10.10:60000'
}
check_wfgen_wav_server() {
    if [ -f "${VIRTUAL_ENV}/bin/proc_query.py" ]; then
        if [[ "$(proc_query.py -type running wav_server.py)" == "True" ]]; then
            if [ -z "${WFGEN_AUDIO_SERVER+x}" ]; then
                export WFGEN_AUDIO_SERVER="tcp://$(proc_query.py -type addr wav_server.py):$(proc_query.py -type port -addr $(proc_query.py -type addr wav_server.py) wav_server.py)"
            fi
            return 0
        fi
    else
        echo "Cannot find wfgen's proc_query.py in ${VIRTUAL_ENV}/bin"
        return 2
    fi
    return 1
}
stop_wfgen_wav_server() {
    killall wav_server.py
    unset WFGEN_AUDIO_SERVER
}

if [ ! -z ${WFGEN_AUDIO_FOLDER+x} ]; then
    check_wfgen_wav_server
    status=$?
    if [[ $status == 1 ]]; then
        start_wfgen_wav_server
    fi
    if [[ $status == 2 ]]; then
        echo "Cannot set WFGEN_AUDIO_SEVER yet."
        echo " -- WFGEN_ENV is not fully configured yet."
        export WFGEN_ENV=0
        (return 0 2>/dev/null) && return 0 || exit 0
    fi
    if [[ $status == 0 ]]; then
        echo "  SUCCESS -- WFGEN_ENV is configured"
        export WFGEN_ENV=1
    fi
else
    export WFGEN_ENV=0
    echo " -- WFGEN_ENV is not fully configured yet."
    (return 0 2>/dev/null) && return 0 || exit 0
fi



