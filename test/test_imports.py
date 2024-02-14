

def import_zmq():
    try:
        import zmq
        return 0
    except ImportError:
        pass
    return 1

def import_socket():
    try:
        import socket
        return 0
    except ImportError:
        pass
    return 1

def import_subprocess():
    try:
        import subprocess
        return 0
    except ImportError:
        pass
    return 1

def import_repo():
    try:
        import wfgen as wg
        return 0
    except ImportError:
        pass
    return 1

def test_imports():
    assert import_zmq() == 0
    assert import_socket() == 0
    assert import_subprocess() == 0
    assert import_repo() == 0

if __name__ == '__main__':
    test_imports()
