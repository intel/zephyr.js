#!/bin/bash
ImageName=CrossToolNG ImageNameExt=${ImageName}.sparseimage
diskutil umount force /Volumes/${ImageName} && true
rm -f ${ImageNameExt} && true
hdiutil create ${ImageName} -volname ${ImageName} -type SPARSE -size 8g -fs HFSX
hdiutil mount ${ImageNameExt}
cd /Volumes/$ImageName
