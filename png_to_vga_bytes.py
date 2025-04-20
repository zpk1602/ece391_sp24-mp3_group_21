import numpy as np
import imageio.v3 as imageio
import sys

image = imageio.imread(sys.argv[1])
print(image.shape)

# vga color scheme, or at least approximately it.
# it doesn't have to be exact, since we just choose the index with the closest
# euclidean norm. thus, it's generally best to preprocess images to already use the vga
# color palette, to ensure quality results.
vga = np.array([
    [0, 0, 0],
    [0, 0, 2/3],
    [0, 2/3, 0],
    [0, 2/3, 2/3],
    [2/3, 0, 0],
    [2/3, 0, 2/3],
    [2/3, 1/3, 0],
    [2/3, 2/3, 2/3],

    [1/3, 1/3, 1/3],
    [1/3, 1/3, 1],
    [1/3, 1, 1/3],
    [1/3, 1, 1],
    [1, 1/3, 1/3],
    [1, 1/3, 1],
    [1, 1, 1/3],
    [1, 1, 1]
    ]) * 255

# resize arrays so we can loop over every pixel and vga color
A = np.tile(image[:,:,:,None], (1,1,1,16))
B = np.tile(vga.T[None,None,:,:], (image.shape[0], image.shape[1], 1, 1))

# take the difference, then the euclidean norm over the 3 color channels
C = np.sum((A-B)**2, axis=2)

# get the index for each pixel, left shift into the appropriate part of the attribute
# byte (the background color specifically), then convert to array of 16 bit ints
D = np.left_shift(np.argmin(C, axis=-1), 12).astype('uint16').tobytes()

f = open(sys.argv[2], 'wb')
f.write(D)
f.close()
