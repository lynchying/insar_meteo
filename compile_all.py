from subprocess import check_output, CalledProcessError, STDOUT
from shlex import split
from os.path import basename
from sys import exit

def cmd(command, ret_out=True):
    try:
        cmd_out = check_output(split(command), stderr=STDOUT)
    except CalledProcessError as e:
        print("Command failed, command: '{}'".format(cmd))
        print("OUTPUT OF THE COMMAND: \n{}".format(e.output.decode()))
        
        ret_code = e.returncode
        print("RETURNCODE: \n{}".format(ret_code))
        
        exit(ret_code)
    if ret_out:
        return cmd_out

def main():
    pass
    

if __name__ == "__main__":
    main()