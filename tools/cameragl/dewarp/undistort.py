import numpy as np
import cv2
import glob
import argparse

def undistort(source, dest):
    K = np.array([[1.00647899e+03, 0.00000000e+00, 1.01263906e+03],
        [0.00000000e+00, 9.98616258e+02, 5.38428063e+02],
        [0.00000000e+00, 0.00000000e+00, 1.00000000e+00]])
    D = np.array([[-0.04836358],
        [ 0.0476189 ],
        [-0.136679  ],
        [ 0.08632943]])

    img =cv2.imread(source)
    map1, map2 = cv2.fisheye.initUndistortRectifyMap(K, D, np.eye(3), K, (1920,1080), cv2.CV_16SC2)
    undistorted_img = cv2.remap(img, map1, map2, interpolation=cv2.INTER_LINEAR, borderMode=cv2.BORDER_CONSTANT)

    cv2.imwrite(dest, undistorted_img)
    print(K)
    print(D)

if __name__ == "__main__":
    parser = argparse.ArgumentParser(prog='undistort.py',
                    description='Stream Studio Dewarper Undistort')
    parser.add_argument('path')
    parser.add_argument('result')

    args = parser.parse_args()
    undistort(args.path, args.result)