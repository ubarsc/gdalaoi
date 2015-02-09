# OGR Driver for Imagine .aoi files #

Build, and set your $GDAL_DRIVER_PATH to the directory containing the .so/.dll.

## Issues ##

* Missing headers in HFA driver lead to duplicated code - need to submit fix to GDAL
* Missing exports for HFA driver in GDAL under Windows means we need to recompile part of GDAL as part of this driver - would be nice to have this code incorporated with GDAL which would make it much cleaner.
* Conversion of ellipsis difficult - at the moment is done by creating 36 polygon segments. Would be nice to set this via metadata or environment variable.