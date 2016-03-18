# OGR Driver for Imagine .aoi files #

Build, and set your $GDAL_DRIVER_PATH to the directory containing the .so/.dll.

## Issues ##

* Missing headers in HFA driver lead to duplicated code - need to submit fix to GDAL
* Missing exports for HFA driver in GDAL under Windows means we need to recompile part of GDAL as part of this driver - would be nice to have this code incorporated with GDAL which would make it much cleaner.
* Number of steps when creating an ellipsis controlled by OGR_AOI_ELLIPSIS_STEPS environment variable or [config](https://trac.osgeo.org/gdal/wiki/ConfigOptions) option. Defaults to 36.