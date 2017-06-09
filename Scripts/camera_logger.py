import win32gui
import win32ui 
import win32con 
import time


def captureImage(ctr):
    hwnd = win32gui.FindWindow(None, 'FpvRenderTarget*')
    wDC = win32gui.GetWindowDC(hwnd)
    dcObj=win32ui.CreateDCFromHandle(wDC)
    cDC=dcObj.CreateCompatibleDC()
    dataBitMap = win32ui.CreateBitmap()
    dataBitMap.CreateCompatibleBitmap(dcObj, 1920, 1080)
    cDC.SelectObject(dataBitMap)
    cDC.BitBlt((0,0),(1920, 1080) , dcObj, (0,0), win32con.SRCCOPY)
    dataBitMap.SaveBitmapFile(cDC, "C:\\Users\\svempral\\Desktop\\lambe"+str(ctr)+".png")
    # Free Resources
    dcObj.DeleteDC()
    cDC.DeleteDC()
    win32gui.ReleaseDC(hwnd, wDC)
    win32gui.DeleteObject(dataBitMap.GetHandle())
    time.sleep(0.1)

ctr = 0
while(1):
    captureImage(ctr)
    ctr = ctr+1