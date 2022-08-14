import numpy as np
from PIL import Image
from matplotlib import colors, cm
import matplotlib.pyplot as plt
import sys

# =================== FILE SETTINGS ===================

isNPY = False

# ==================== DFT SETTINGS ===================

# If true, writes out the individual 2D slices of the NPY file as Png files
outputNPYAsPng = False

# If true, rotates the source images 90 degrees and does the process again, to show the Z (time) axis
doXZ = False

# If true, takes a 3D dft and shows slice [:,:,0]
do3DSlice = False

# If true, zeroes out DC
removeDC = True

# If true, writes out DFTs for individual slices of the texture
makeIndividualDFTs = False

# If true, will output greyscale DFT images
DFTGreyscale = False

# If true, will output color map DFT images
DFTColorMap  = True

# If true, will put the dft magnitude through log
DFTLog = True

# Multiplies the dft magnitudes by this scale. This happens after the optional log and before the normalization / clipping.
DFTScale = 1.0

# If true, will normalize the DFT, otherwise will clip it
DFTNormalize = False

# If true, will force the color map to have a y axis matching DFTClipMin / DFTClipMax
# Useful for making DFTs that can be compared
ForceColorMapYAxis = True

# The range clipped to. greyscale images will always be clipped to [0,1], but the color map can go outside of that range
DFTClipMin = 0.0
DFTClipMax = 1.0

# If true, averages all slices of the DFT to give an expected DFT, which can help see DFT structures with less noise.
averagAll = True

# =====================================================

def SaveDFTGreyscale(dft, fileName):
    # Normalize or clip, as desired
    if DFTNormalize:
        themin = np.min(dft)
        themax = np.max(dft)
        if themin != themax:
            dft2 = (dft - themin) / (themax - themin)
        else:
            dft2 = dft
    else:
        dft2 = np.clip(dft, DFTClipMin, DFTClipMax)
    dft2 = np.clip(dft2, 0.0, 1.0)

    if len(dft2.shape) != 2:
        sys.exit("Wrong shaped image to DFT, but script could be updated to support it.")
    else:
        Image.fromarray((dft2*255.0).astype(np.uint8), mode="L").save(fileName);

def SaveDFTColorMap(dft, title, outFileName):

    print("actual min/max of " + outFileName + " = (" + str(np.min(dft)) + ", " + str(np.max(dft)) + ")")

    # Normalize or clip, as desired
    #if DFTNormalize:
    #    themin = np.min(dft)
    #    themax = np.max(dft)
    #    if themin != themax:
    #        dft2 = (dft - themin) / (themax - themin)
    #    else:
    #        dft2 = dft
    #else:
    #    dft2 = np.clip(dft, DFTClipMin, DFTClipMax)

    dft2 = dft
    fig = plt.figure(figsize=(8,6))
    if ForceColorMapYAxis:
        plt.imshow(dft2,cmap="viridis", vmin=DFTClipMin, vmax=DFTClipMax)
    else:
        plt.imshow(dft2,cmap="viridis")
    plt.title(title)
    plt.colorbar()
    plt.savefig(outFileName)

def Process(samples, fileName):

    # Write out the individual slice images if we should
    if isNPY and outputNPYAsPng:
        for i in range(samples.shape[2]):
            Image.fromarray(np.uint8(samples[:,:,i] * 255), 'L').save(fileName + str(i) + ".png")

    # make the full file name
    fileNameSuffix = ".dft"
    if DFTLog:
        fileNameSuffix += ".log"
    if DFTNormalize:
        fileNameSuffix += ".normalized"

    # 0hz slice of 3D DFT
    if do3DSlice:
        dft3d = abs(np.fft.fftn(samples))
        if removeDC:
            dft3d[0,0,0] = 0.0
        if DFTLog:
            dft3d = np.log(1+dft3d)
        dft3d = dft3d * DFTScale
        dft3d = np.fft.fftshift(dft3d)

        if DFTGreyscale:
            SaveDFTGreyscale(dft3d[:,:,int(dft3d.shape[2]/2)], fileName + fileNameSuffix + ".grey.3d.0hz.png")

        if DFTColorMap:
            SaveDFTColorMap(dft3d[:,:,int(dft3d.shape[2]/2)], "3DDFT[:,:,0.5] of " + fileName , fileName + fileNameSuffix + ".colormap.3d.0hz.png")

    # 2D DFTs of slices, combined
    combinedDFT = samples.copy()
    for i in range(samples.shape[2]):
        dft = abs(np.fft.fft2(samples[:,:,i]))
        if removeDC:
            dft[0,0] = 0.0
        if DFTLog:
            dft = np.log(1+dft)
        dft = dft * DFTScale
        combinedDFT[:,:,i] = np.fft.fftshift(dft)

        if makeIndividualDFTs:
            if DFTGreyscale:
                SaveDFTGreyscale(combinedDFT[:,:,i], fileName + fileNameSuffix + ".grey." + str(i) + ".png")

            if DFTColorMap:
                SaveDFTColorMap(combinedDFT[:,:,i], "DFT of " + fileName + "["+str(i)+"]", fileName + fileNameSuffix + ".colormap." + str(i) + ".png")

    if averagAll:
        avgDFTAll = []

        for i in range(samples.shape[2]):
            if i == 0:
                avgDFTAll = combinedDFT[:,:,i]
            else:
                avgDFTAll += combinedDFT[:,:,i]

        avgDFTAll /= samples.shape[2]

        if DFTGreyscale:
            SaveDFTGreyscale(avgDFTAll, fileName + fileNameSuffix + ".grey.dftavg.png")

        if DFTColorMap:
            SaveDFTColorMap(avgDFTAll, "Avg DFT of " + fileName, fileName + fileNameSuffix + ".colormap.dftavg.png")

# ======================= MAIN =======================

if len(sys.argv) < 3:
    print('Usage: python MultiDFT.py fileNameBase count <DFTClipMin DFTClipMax>')
    sys.exit()

fileNameBase = sys.argv[1]
fileCount = int(sys.argv[2])

if len(sys.argv) >= 5:
    DFTClipMin = float(sys.argv[3])
    DFTClipMax = float(sys.argv[4])
    ForceColorMapYAxis = True
    DFTNormalize = False
else:
    ForceColorMapYAxis = False
    DFTNormalize = True

samples = []

if isNPY:
    samples = np.load(fileNameBase + '.npy') / 255.0
else:
    for i in range(fileCount):
        fileName = fileNameBase.replace("%i", str(i))

        print(fileName)
        im = np.array(Image.open(fileName), dtype=float) / 255.0

        if len(im.shape) == 3 and im.shape[2] != 1:
            sys.exit(fileName + " is not a single channel image. Script needs to be modified to support it");
        
        im = im.reshape(im.shape[0], im.shape[1], 1)
        if i == 0:
            samples = im
        else:
            samples = np.append(samples, im, axis=2)

Process(samples, fileNameBase.replace("%i", "all") + ".XY")

if doXZ:    
    samples = np.rot90(samples, axes=(0,2))
    Process(samples, fileNameBase.replace("%i", "all") + ".XZ")
