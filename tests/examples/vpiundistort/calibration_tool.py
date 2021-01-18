#!/usr/bin/env python3

"""
Copyright (c) 2020, RidgeRun  All rights reserved.

The contents of this software are proprietary and confidential to RidgeRun,
LLC.  No part of this program may be photocopied, reproduced or translated
into another programming language without prior written consent of
RidgeRun, LLC.  The user is free to modify the source code after obtaining
a software license from RidgeRun.  All source code changes must be provided
back to RidgeRun without any encumbrance.
"""

""" Tool to calibrate a single camera """

import argparse
import glob
import json
import os.path
import subprocess
import sys
import time

import cv2
import numpy as np

global SETTINGS

IMG_EXTENSION = ".png"

TEXT_FORMATS = {"YELLOW": "\033[93m",
                "BOLD": "\033[1m",
                "NONE": "\033[0m"}

COLOR_RED = (0, 0, 255)
COLOR_BLUE = (255, 0, 0)
COLOR_GREEN = (0, 255, 0)

CAPTURE_TIMEOUT = 2  # Timeout in seconds
CAPTURE_TEXT_POSITION = (20, 50)
CAPTURE_TEXT_SIZE = 1
CAPTURE_TEXT_BORDER = 2

EXIT_INFO_TEXT_POSITION = (20, 100)

SETTINGS = {"camIndex": 0, 
            "brownConradyCameraMatrix": 0, 
            "fisheyeCameraMatrix": 0, 
            "brownConradyDistortionParameters": 0,
            "fisheyeDistortionParameters": 0}


def load_settings():
    """
    Loads configuration from json file.
    """

    global SETTINGS

    with open(args.settings_path) as json_file:
        data = json.load(json_file)

        for key in SETTINGS:
            SETTINGS[key] = data[key]

        SETTINGS["cameraWidth"] = int(data["cameraWidth"])
        SETTINGS["cameraHeight"] = int(data["cameraHeight"])
        SETTINGS["chessboardInnerCornersWidth"] = int(data["chessboardInnerCornersWidth"])
        SETTINGS["chessboardInnerCornersHeight"] = int(data["chessboardInnerCornersHeight"])


def save_settings():
    """
    Saves configuration to json file.
    """

    data = {}
    data["cameraWidth"] = int(SETTINGS["cameraWidth"])
    data["cameraHeight"] = int(SETTINGS["cameraHeight"])
    data["chessboardInnerCornersWidth"] = int(SETTINGS["chessboardInnerCornersWidth"])
    data["chessboardInnerCornersHeight"] = int(SETTINGS["chessboardInnerCornersHeight"])

    for key in SETTINGS:
        data[key] = SETTINGS[key]

    with open(args.settings_path, "w") as out_file:
        json_indentation_level = 4
        json.dump(data, out_file, indent=json_indentation_level)


def delete_files_in_dir(dir_name, extension):
    """
    Deletes all files of defined extension in a directory.

    Args:
        dir_name (string): Directory path.
        extension (string): Extension of the files to delete.
    """

    filelist = [f for f in os.listdir(dir_name) if f.endswith(extension)]
    for f in filelist:
        os.remove(os.path.join(dir_name, f))


def verify_frame(input_frame):
    """
    Verifies if input frame contains valid chessboard.

    Args:
        input_frame (np.array): OpenCV frame to analyze.

    Returns:
        bool: True if chessboard was found on frame, False otherwise.
    """

    # Convert to gray in case the image has multiple channels
    if len(input_frame.shape) == 3:
        gray = cv2.cvtColor(input_frame, cv2.COLOR_BGR2GRAY)
    else:
        gray = input_frame

    ret, corners = cv2.findChessboardCorners(gray, (int(SETTINGS["chessboardInnerCornersHeight"]),
                                                    int(SETTINGS["chessboardInnerCornersWidth"])),
                                             cv2.CALIB_CB_ADAPTIVE_THRESH +
                                             cv2.CALIB_CB_FAST_CHECK +
                                             cv2.CALIB_CB_NORMALIZE_IMAGE)

    return ret, corners


def get_capture():
    """
    Selects and returns a video capture element.

    Returns:
        cv2.VideoCapture: video capture element from which pictures can be
            extracted using the selected camera.

    Raises:
        RuntimeError: if can't open the camera.
    """
    cam_index = int(SETTINGS["camIndex"])
    width = int(SETTINGS["cameraWidth"])
    height = int(SETTINGS["cameraHeight"])
    gst_elements = str(subprocess.check_output("gst-inspect-1.0"))

    if args.v4l2:
        gst_str = ("v4l2src device=/dev/video{} ! "
                   "video/x-raw, width=(int){}, height=(int){} ! "
                   "videoconvert ! appsink").format(cam_index, width, height)
    elif "nvcamerasrc" in gst_elements:
        gst_str = ("nvcamerasrc sensor-id={} ! "
                   "nvvidconv ! "
                   "video/x-raw, width=(int){}, height=(int){}, "
                   "format=(string)BGRx ! "
                   "videoconvert ! appsink").format(cam_index, width, height)
    elif "nvarguscamerasrc" in gst_elements:
        gst_str = ("nvarguscamerasrc sensor-id={} ! "
                   "nvvidconv ! "
                   "video/x-raw, width=(int){}, height=(int){}, "
                   "format=(string)BGRx ! "
                   "videoconvert ! appsink").format(cam_index, width, height)
    else:
        raise RuntimeError("Can\'t open camera.")

    return cv2.VideoCapture(gst_str, cv2.CAP_GSTREAMER)


def process_frames(cam_index):
    """
    Process input frames from camera.

    Args:
        cam_index (int): Device index of the camera to process (/dev/videoX).

    Raises:
        RuntimeError: if capture device is None or couldn't be opened.
        RuntimeError: if the path to save the images is not empty and the
            --remove flag was not set.
    """

    width = int(SETTINGS["cameraWidth"])
    height = int(SETTINGS["cameraHeight"])

    capture = get_capture()

    if (capture is None) or (not capture.isOpened()):
        error_msg = "Unable to open video source: dev/video" + str(cam_index)
        raise RuntimeError(error_msg)

    cv2.namedWindow("Capture", cv2.WINDOW_NORMAL)
    cv2.resizeWindow("Capture", width, height)

    curr_image_name = ""
    frame_counter = 0

    if args.remove:
        print("Deleting old calibration images")
        delete_files_in_dir(args.images_path, IMG_EXTENSION)
    else:
        try:
            if len(os.listdir(args.images_path)) != 0:
                error_msg = ("Destination directory not empty."
                             "Please save files or run with -r option")
                raise RuntimeError(error_msg)

        except FileNotFoundError:
            os.makedirs(args.images_path)

    start = time.time()

    print("Capturing. Press (q) to finish.")
    while(True):
        ret, frame = capture.read()

        # Resize image to improve function performance
        status, corners = verify_frame(frame)

        if status:
            end = time.time()
            status_color = COLOR_GREEN  # Green color to notify success

            if (end - start) > CAPTURE_TIMEOUT:  # Timeout to avoid lots of images
                start = time.time()

                curr_image_name = args.images_path + "/"
                curr_image_name += "frame" + \
                    "%04d" % (int(frame_counter)) + IMG_EXTENSION

                cv2.imwrite(curr_image_name, frame)
                frame_counter += 1

            p0 = 0
            p1 = int(SETTINGS["chessboardInnerCornersHeight"]) - 1
            p2 = (int(SETTINGS["chessboardInnerCornersHeight"]) *
                  int(SETTINGS["chessboardInnerCornersWidth"])) - \
                  int(SETTINGS["chessboardInnerCornersHeight"])
            p3 = (int(SETTINGS["chessboardInnerCornersHeight"]) *
                  int(SETTINGS["chessboardInnerCornersWidth"])) - 1

            # Draw chessboard corners
            thickness = 2
            radius = 10
            for p in [p0, p1, p2, p3]:
                center = (corners[p][0][0], corners[p][0][1])
                cv2.circle(frame, center, radius, COLOR_BLUE, thickness)

        else:
            status_color = COLOR_RED  # Red color to notify fail

        # Show the number of saved images
        saved_images_info = "Saved images: " + str(frame_counter)
        cv2.putText(
            frame,
            saved_images_info,
            CAPTURE_TEXT_POSITION,
            cv2.FONT_HERSHEY_SIMPLEX,
            CAPTURE_TEXT_SIZE,
            status_color,
            CAPTURE_TEXT_BORDER)

        # Add exit info
        exit_info_text = "Press (q) to finish"
        cv2.putText(
            frame,
            exit_info_text,
            EXIT_INFO_TEXT_POSITION,
            cv2.FONT_HERSHEY_SIMPLEX,
            CAPTURE_TEXT_SIZE,
            status_color,
            CAPTURE_TEXT_BORDER)

        thickness = 20
        cv2.rectangle(frame, (0, 0), (width, height), status_color, thickness)
        cv2.imshow("Capture", frame)

        if cv2.waitKey(1) & 0xFF == ord("q"):
            break

    capture.release()
    cv2.destroyAllWindows()


def calculate_distortion_parameters(cam_index):
    """
    Computes the distortion parameters for both Fisheye and Brown-Conrady models.

    Args:
        cam_index (int): Device index of the camera to process (/dev/videoX).

    Raises:
        RuntimeError: if there are no images in the calibration images
            directory.
    """

    checkerboard = (int(SETTINGS["chessboardInnerCornersHeight"]), int(
        SETTINGS["chessboardInnerCornersWidth"]))
    img_shape = (int(SETTINGS["cameraWidth"]), int(SETTINGS["cameraHeight"]))

    subpix_max_count = 30
    subpix_epsilon = 0.1
    subpix_criteria = (
        cv2.TERM_CRITERIA_EPS + cv2.TERM_CRITERIA_MAX_ITER,
        subpix_max_count,
        subpix_epsilon)

    objp = np.zeros((1, checkerboard[0] * checkerboard[1], 3), np.float32)
    objp[0, :, :2] = np.mgrid[0:checkerboard[0],
                              0:checkerboard[1]].T.reshape(-1, 2)

    fisheye_calibration_flags = cv2.fisheye.CALIB_RECOMPUTE_EXTRINSIC + \
        cv2.fisheye.CALIB_FIX_SKEW

    brown_conrady_calibration_flags = cv2.CALIB_RATIONAL_MODEL

    obj_points = []  # 3d point in real world space
    img_points = []  # 2d points in image

    curr_path = args.images_path
    images = glob.glob(curr_path + "/*" + IMG_EXTENSION)

    if len(images) == 0:
        error_msg = "Could not find images at: " + curr_path
        raise RuntimeError(error_msg)

    print(
        "Calculating K/D parameters for device: /dev/video" +
        str(cam_index) +
        " ...")
    for fname in images:
        img = cv2.imread(fname)

        # Convert to gray in case the image has multiple channels
        if len(img.shape) == 3:
            gray = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)
        else:
            gray = img

        # Find the chess board corners
        ret, corners = verify_frame(img)
        # If found, add object points, image points (after refining them)
        if not ret:
            print("Could not find valid chessboard pattern in " + str(fname))
        else:
            obj_points.append(objp)
            cv2.cornerSubPix(gray, corners, (3, 3), (-1, -1), subpix_criteria)
            img_points.append(corners)

    camera_matrix_shape = (3, 3)
    num_fisheye_distortion_params = 4
    num_brown_conrady_distortion_params = 8
    rvecs_shape = (1, 1, 3)
    tvecs_shape = (1, 1, 3)

    fisheye_distortion_params_shape = (num_fisheye_distortion_params, 1)
    brown_conrady_distortion_params_shape = (
        num_brown_conrady_distortion_params, 1)

    camera_matrix = np.zeros(camera_matrix_shape)
    fisheye_distortion_params = np.zeros(fisheye_distortion_params_shape)
    brown_conrady_distortion_params = np.zeros(
        brown_conrady_distortion_params_shape)

    rvecs = [np.zeros(rvecs_shape, dtype=np.float64)
             for i in range(len(obj_points))]
    tvecs = [np.zeros(tvecs_shape, dtype=np.float64)
             for i in range(len(obj_points))]

    max_count = 30
    epsilon = 1e-6
    criteria = (
        cv2.TERM_CRITERIA_EPS + cv2.TERM_CRITERIA_MAX_ITER,
        max_count,
        epsilon)

    # Get camera matrix and fisheye distortion parameters
    fisheye_calibration_results = cv2.fisheye.calibrate(
        obj_points,
        img_points,
        img_shape,
        camera_matrix,
        fisheye_distortion_params,
        rvecs,
        tvecs,
        fisheye_calibration_flags,
        criteria)

    fisheye_camera_matrix = fisheye_calibration_results[1]
    fisheye_distortion_params = fisheye_calibration_results[2]

    # Get camera matrix and Brown Conrady distortion parameters
    brown_conrady_calibration_results = cv2.calibrateCamera(
        obj_points,
        img_points,
        img_shape,
        camera_matrix,
        brown_conrady_distortion_params,
        rvecs,
        tvecs,
        brown_conrady_calibration_flags,
        criteria)

    brown_conrady_camera_matrix = brown_conrady_calibration_results[1]
    brown_conrady_distortion_params = brown_conrady_calibration_results[2]

    # Get only first 8 values of Brown Conrady distortion parameters
    brown_conrady_distortion_params = brown_conrady_distortion_params[0:8]

    SETTINGS["fisheyeCameraMatrix"] = fisheye_camera_matrix.tolist()
    SETTINGS["brownConradyCameraMatrix"] = brown_conrady_camera_matrix.tolist()
    SETTINGS["fisheyeDistortionParameters"] = fisheye_distortion_params.tolist()
    SETTINGS["brownConradyDistortionParameters"] = brown_conrady_distortion_params.tolist()

    save_settings()


def test_undistort(cam_index):
    """
    Runs the undistortion test of the computed parameters.

    Args:
        cam_index (int): Device index of the camera to process (/dev/videoX).

    Raises:
        RuntimeError: if capture device is None or couldn't be opened.
    """

    fisheye_camera_matrix = np.array(SETTINGS["fisheyeCameraMatrix"])
    brown_conrady_camera_matrix = np.array(SETTINGS["brownConradyCameraMatrix"])
    fisheye_distortion_params = np.array(
        SETTINGS["fisheyeDistortionParameters"])
    brown_conrady_distortion_params = np.array(
        SETTINGS["brownConradyDistortionParameters"])
    img_shape = (int(SETTINGS["cameraWidth"]), int(SETTINGS["cameraHeight"]))

    width = int(SETTINGS["cameraWidth"])
    height = int(SETTINGS["cameraHeight"])

    capture = get_capture()

    if (capture is None) or (not capture.isOpened()):
        error_msg = "Unable to open video source: dev/video" + str(cam_index)
        raise RuntimeError(error_msg)

    cv2.namedWindow("Capture", cv2.WINDOW_NORMAL)
    cv2.resizeWindow("Capture", width, height)

    print("Testing. Press (q) to finish.")
    while(True):
        ret, img = capture.read()

        # Fisheye Undistort
        balance = 0
        new_camera_matrix = cv2.fisheye.estimateNewCameraMatrixForUndistortRectify(
            fisheye_camera_matrix, fisheye_distortion_params, img_shape,
            np.eye(3), balance=balance)

        map1, map2 = cv2.fisheye.initUndistortRectifyMap(
            fisheye_camera_matrix, fisheye_distortion_params, np.eye(3),
            new_camera_matrix, img_shape, cv2.CV_16SC2)

        fisheye_undistort_img = cv2.remap(img,
                                          map1,
                                          map2,
                                          interpolation=cv2.INTER_LINEAR,
                                          borderMode=cv2.BORDER_CONSTANT)

        fisheye_undistort_img = cv2.putText(fisheye_undistort_img,
                                            "Undistorted (fisheye)",
                                            CAPTURE_TEXT_POSITION, 0,
                                            CAPTURE_TEXT_SIZE, (200, 0, 0),
                                            2, cv2.LINE_AA)

        # Brown Conrady Undistort
        brown_conrady_undistort_img = cv2.undistort(
            img, brown_conrady_camera_matrix, brown_conrady_distortion_params)
        brown_conrady_undistort_img = cv2.putText(brown_conrady_undistort_img,
                                                  "Undistorted (Brown-Conrady)",
                                                  CAPTURE_TEXT_POSITION, 0,
                                                  CAPTURE_TEXT_SIZE,
                                                  (200, 0, 0),
                                                  2, cv2.LINE_AA)

        # Put text on original image
        img = cv2.putText(img,
                          "Original",
                          CAPTURE_TEXT_POSITION, 0,
                          CAPTURE_TEXT_SIZE, (200, 0, 0),
                          2, cv2.LINE_AA)

        # Add exit info
        exit_info_text = "Press (q) to finish"
        cv2.putText(
            img,
            exit_info_text,
            EXIT_INFO_TEXT_POSITION,
            cv2.FONT_HERSHEY_SIMPLEX,
            CAPTURE_TEXT_SIZE,
            COLOR_RED)

        # Show comparison of undistorted images
        undistort_comparison = cv2.hconcat(
            [img, fisheye_undistort_img, brown_conrady_undistort_img])
        cv2.imshow("Capture", undistort_comparison)

        if cv2.waitKey(1) & 0xFF == ord("q"):
            break

    capture.release()
    cv2.destroyAllWindows()


def calibrate():
    """
    Calibrates the cameras by getting the frames and computing the distortion.
        parameters.
    """

    process_frames(int(SETTINGS["camIndex"]))
    calculate_distortion_parameters(int(SETTINGS["camIndex"]))

def print_cam_parameters():
    """
    Prints the computed camera parameters: camera matrix and distortion
        parameters for both Fisheye and Brown-Conrady models.
    """
    intrinsic_matrix = np.array(SETTINGS["fisheyeCameraMatrix"]).flatten()
    fisheye_params = np.array(SETTINGS["fisheyeDistortionParameters"]).flatten()
    brown_conrady_params = np.array(
        SETTINGS["brownConradyDistortionParameters"]).flatten()

    intrinsic_str = "intrinsic=\"<<%.3f, %.3f, %.3f>, <%.3f, %.3f, %.3f>>\"" % (
        intrinsic_matrix[0], intrinsic_matrix[1], intrinsic_matrix[2],
        intrinsic_matrix[3], intrinsic_matrix[4], intrinsic_matrix[5])
    
    fisheye_str = ("model=fisheye k1=%.3f k2=%.3f k3=%.3f k4=%.3f" 
        % (fisheye_params[0], fisheye_params[1], fisheye_params[2],
        fisheye_params[3]))
    
    brown_conrady_str = ("model=polynomial k1=%.3f k2=%.3f p1=%.3f p2=%.3f "
        + "k3=%.3f k4=%.3f k5=%.3f k6=%.3f") % (brown_conrady_params[0],
        brown_conrady_params[1], brown_conrady_params[2], 
        brown_conrady_params[3], brown_conrady_params[4],
        brown_conrady_params[5], brown_conrady_params[6],
        brown_conrady_params[7])

    print("\n" + TEXT_FORMATS["YELLOW"] + TEXT_FORMATS["BOLD"],
          " ============================================= \n",
          "|  The following variables can be used as the |\n",
          "| properties of the vpiundistort element.     |\n",
          "| Use the parameters of your prefered model.  |\n",
          " ============================================= \n",
          TEXT_FORMATS["NONE"])

    print(TEXT_FORMATS["YELLOW"] + TEXT_FORMATS["BOLD"],
          "\n# =======",
          "\n# FISHEYE",
          "\n# =======",
          TEXT_FORMATS["NONE"])
    
    print(fisheye_str + " " + intrinsic_str)

    print(TEXT_FORMATS["YELLOW"] + TEXT_FORMATS["BOLD"],
          "\n# =============",
          "\n# BROWN_CONRADY",
          "\n# =============",
          TEXT_FORMATS["NONE"])
    print(brown_conrady_str + " " + intrinsic_str)
    print()


def main(argv):
    global args

    parser = argparse.ArgumentParser()
    parser.add_argument("-c", "--calibrate", type=bool, nargs="?",
                        const=True, default=False,
                        help="Calibrate distortion parameters")
    parser.add_argument("-v", "--v4l2", type=bool, nargs="?",
                        const=True, default=False,
                        help="Use v4l2 to read from camera. "
                             "Otherwise, NVIDIA plugins will be used.")
    parser.add_argument("-r", "--remove", type=bool, nargs="?",
                        const=True, default=False,
                        help="Allow to delete old images")
    parser.add_argument("-p", "--images_path", default="imgs/calibration/",
                        help="Path to save images")
    parser.add_argument("-s", "--settings_path", default="settings.json",
                        help="Path to settings file")
    parser.add_argument("-t", "--test", type=bool, nargs="?",
                        const=True, default=False, help="Test calibration")
    args = parser.parse_args()

    # Load initial reference from json file
    load_settings()

    # Calculate and save the distortion parameters
    if args.calibrate:
        calibrate()

    # Test undistortion and exit
    if args.test:
        test_undistort(int(SETTINGS["camIndex"]))

    if args.test or args.calibrate:
        # Print camera environment variables
        print_cam_parameters()

if __name__ == "__main__":
    main(sys.argv[1:])
