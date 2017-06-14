import win32gui, win32ui, win32con

from PythonClient import *
import sys
import time
import msgpackrpc
import math
import os

client = AirSimClient('127.0.0.1')
logFilePath = os.path.dirname(os.path.abspath(__file__)) + "\\groundtruth.txt"

def captureImage(type, ctr):
    print("Capturing")
    if type == 'Scene':
        hwnd = win32gui.FindWindow(None, 'FpvRenderTarget')
        savePath = os.path.dirname(os.path.abspath(__file__)) + "\\scene\\image" + str(ctr) + ".png"
    elif type == 'Depth':
        hwnd = win32gui.FindWindow(None, 'DepthRenderTarget')
        savePath = os.path.dirname(os.path.abspath(__file__)) + "\\depth\\image" + str(ctr) + ".png"    
    elif type == 'Seg':
        hwnd = win32gui.FindWindow(None, 'SegmentationRenderTarget')
        savePath = os.path.dirname(os.path.abspath(__file__)) + "\\seg\\image" + str(ctr) + ".png"  

    wDC = win32gui.GetWindowDC(hwnd)
    dcObj=win32ui.CreateDCFromHandle(wDC)
    cDC=dcObj.CreateCompatibleDC()
    dataBitMap = win32ui.CreateBitmap()
    dataBitMap.CreateCompatibleBitmap(dcObj, 1280, 720)
    cDC.SelectObject(dataBitMap)
    cDC.BitBlt((0,0),(1280, 720) , dcObj, (0,0), win32con.SRCCOPY)
    dataBitMap.SaveBitmapFile(cDC, savePath)

    rpy = client.getRollPitchYaw()
    with open(logFilePath, "a") as logFile:
        logFile.write(str(rpy))
        logFile.write("\n")
    # Free Resources
    dcObj.DeleteDC()
    cDC.DeleteDC()
    win32gui.ReleaseDC(hwnd, wDC)
    win32gui.DeleteObject(dataBitMap.GetHandle())
    time.sleep(0.001)

if not os.path.exists(os.path.dirname(os.path.abspath(__file__)) + "\\scene") :
    os.makedirs(os.path.dirname(os.path.abspath(__file__)) + "\\scene")
if not os.path.exists(os.path.dirname(os.path.abspath(__file__)) + "\\depth") :
    os.makedirs(os.path.dirname(os.path.abspath(__file__)) + "\\depth")
if not os.path.exists(os.path.dirname(os.path.abspath(__file__)) + "\\seg") :
    os.makedirs(os.path.dirname(os.path.abspath(__file__)) + "\\seg")

file = open(logFilePath, "w+")
file.write("Ground Truth")
file.write("\n")
file.close() 
ctr = 0

t_end = time.time() + 5
while time.time() < t_end:
    captureImage('Scene', ctr)
    captureImage('Depth', ctr)
    captureImage('Seg', ctr)
    ctr = ctr+1