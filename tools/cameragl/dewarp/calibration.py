import numpy as np
import cv2
import glob
import argparse

def calibrate(path, validation_image, validation_image_dewarped):
    """
        :path : jpeg files path
        :validation_image : validation image to test dewarping
        :validation_image_dewarped : path to dewarped image
    """
    # Define the chess board rows and columns
    CHECKERBOARD = (6,9)
    subpix_criteria = (cv2.TERM_CRITERIA_EPS + cv2.TERM_CRITERIA_MAX_ITER, 30, 0.1)
    calibration_flags = cv2.fisheye.CALIB_RECOMPUTE_EXTRINSIC +  cv2.fisheye.CALIB_FIX_SKEW
    objp = np.zeros((1, CHECKERBOARD[0]*CHECKERBOARD[1], 3), np.float32)
    objp[0,:,:2] = np.mgrid[0:CHECKERBOARD[0], 0:CHECKERBOARD[1]].T.reshape(-1, 2)
    gray=None
    objpoints = [] # 3d point in real world space
    imgpoints = [] # 2d points in image plane.
    counter = 0

    files="{}/*.jpeg".format(path)
    print("Parsing path : {}, ({})".format(path, files))
    for path in glob.glob(files):
        print(path)
        # Load the image and convert it to gray scale
        img = cv2.imread(path)
        gray = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)

        # Find the chess board corners
        ret, corners = cv2.findChessboardCorners(gray, CHECKERBOARD, cv2.CALIB_CB_ADAPTIVE_THRESH+cv2.CALIB_CB_FAST_CHECK+cv2.CALIB_CB_NORMALIZE_IMAGE)

        # Make sure the chess board pattern was found in the image
        if ret:
            objpoints.append(objp)
            cv2.cornerSubPix(gray,corners,(3,3),(-1,-1),subpix_criteria)
            imgpoints.append(corners)
        counter+=1

    N_imm = counter# number of calibration images
    K = np.zeros((3, 3))
    D = np.zeros((4, 1))
    rvecs = [np.zeros((1, 1, 3), dtype=np.float64) for i in range(N_imm)]
    tvecs = [np.zeros((1, 1, 3), dtype=np.float64) for i in range(N_imm)]
    rms, _, _, _, _ = cv2.fisheye.calibrate(
        objpoints,
        imgpoints,
        gray.shape[::-1],
        K,
        D,
        rvecs,
        tvecs,
        calibration_flags,
        (cv2.TERM_CRITERIA_EPS+cv2.TERM_CRITERIA_MAX_ITER, 30, 1e-6))

    img =cv2.imread(validation_image)
    map1, map2 = cv2.fisheye.initUndistortRectifyMap(K, D, np.eye(3), K, (1920,1080), cv2.CV_16SC2)
    undistorted_img = cv2.remap(img, map1, map2, interpolation=cv2.INTER_LINEAR, borderMode=cv2.BORDER_CONSTANT)
    
    cv2.imwrite(validation_image_dewarped, undistorted_img)
    print(K)
    print(D)
    print(map1)
    print(map2)
if __name__ == "__main__":
    parser = argparse.ArgumentParser(prog='calibration.py',
                    description='Stream Studio Dewarper Calibration')
    parser.add_argument('path')
    parser.add_argument('validation_image')
    parser.add_argument('validation_image_dewarped')

    args = parser.parse_args()
    calibrate(args.path, args.validation_image, args.validation_image_dewarped)