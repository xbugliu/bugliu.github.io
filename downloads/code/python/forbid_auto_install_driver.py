import os
import time
import subprocess

def RunLoop():
    while(True):
        startupinfo = subprocess.STARTUPINFO()
        startupinfo.dwFlags |= subprocess.STARTF_USESHOWWINDOW
        subprocess.call('taskkill /F /IM drvinst.exe', startupinfo=startupinfo)
        time.sleep(0.5)


if __name__ == "__main__":
    RunLoop()
