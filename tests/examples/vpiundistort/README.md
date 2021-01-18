# RidgeRun Calibration Tool

The **vpiundistort** element requires the calibration parameters and the
distortion coefficients of the input camera. In order to extract them, we
provide a tool that will simplify this process. Check the next section to learn
how to use it.

## How to calibrate the camera

1. Open the *settings.json* file in this directory, and edit the following lines:

```
"cameraWidth": 1280,
"cameraHeight": 800,
"chessboardInnerCornersWidth": 6,
"chessboardInnerCornersHeight": 9,
"camIndex": 0,
```

with the following values:

* **cameraWidth**: your camera resolution width.
* **cameraHeight**: your camera resolution height.
* **chessboardInnerCornersWidth**: Number of inner corners in the calibration.
  pattern counted horizontally.*
* **chessboardInnerCornersHeight**: Number of inner corners in the calibration.
  pattern counted vertically.*
* **camIndex**: The identifier number of your camera following the *video* word when you run `ls /dev/video*`.

*If you use the calibration pattern we provide in *imgs/pattern.png*, there is
no need to modify the **chessboardInnerCornersWidth** and
**chessboardInnerCornersHeight** options.

2. Run the calibrator. The following options are available for the script:

* `-c`: Execute the calibration process.
* `-p $IMAGE_PATH`: Path to store the calibration images. Default: *imgs/calibration*.
* `-r`: Remove the images from the `$IMAGE_PATH`.
* `-s $SETTINGS_PATH`: Path to the settings file. Default: *settings.json*.
* `-t`: Run a test routine to see the calibration results.
* `-v`: Use the vl42 plugin to capture images. By default the NVIDIA plugins will be used.

The easiest way to calibrate the camera is to run:

```
python3 calibration_tool.py -c -r
```

But feel free to explore the other options in case you need it.

3. Now, a window with the camera image will appear. You must show the
   calibration pattern to the camera until the frame turns green, which means
   that the pattern has been detected (see animation below).

4. Move the pattern to fully cover the area and tilt it in both horizontal and
   vertical directions, as shown in the animation, to obtain a good calibration.

<img src="https://developer.ridgerun.com/wiki/images/3/32/Single_Camera_Calibration_Success_Examples_Animation.gif">

5. Once the saved images counter has reached 40 ~ 50 images, you may press the
   `q` key to obtain the string with the parameters ready to be used in a
   GStreamer pipeline as properties of the **vpiundistort** element. This will
   be shown in your console in a format like this:

```
  ============================================= 
 |  The following variables can be used as the |
 | properties of the vpiundistort element.     |
 | Use the parameters of your prefered model.  |
  ============================================= 
 
 
# ======= 
# FISHEYE 
# ======= 
model=fisheye k1=0.053 k2=0.013 k3=0.001 k4=-0.002 intrinsic="<<305.950, 0.000, 654.029>, <0.000, 305.226, 415.647>>"
 
# ============= 
# BROWN_CONRADY 
# ============= 
model=polynomial k1=0.522 k2=-0.016 p1=-0.000 p2=0.000 k3=-0.002 k4=0.801 k5=0.052 k6=-0.008 intrinsic="<<305.950, 0.000, 654.029>, <0.000, 305.226, 415.647>>"
```

## How to use in a GStreamer pipeline

Use the **vpiundistort** element with the extracted parameters like this:
```
! vpiundistort model=fisheye k1=0.053 k2=0.013 k3=0.001 k4=-0.002 intrinsic="<<305.950, 0.000, 654.029>, <0.000, 305.226, 415.647>>" !
```
```
! vpiundistort model=polynomial k1=0.522 k2=-0.016 p1=-0.000 p2=0.000 k3=-0.002 k4=0.801 k5=0.052 k6=-0.008 intrinsic="<<305.950, 0.000, 654.029>, <0.000, 305.226, 415.647>>" !
```
