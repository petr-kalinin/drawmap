#!/bin/bash

mkdir -p srtm

filename=$1

if [ ! -e "srtm/$filename.hgt" ]; then
    wget "http://e4ftl01.cr.usgs.gov/SRTM/SRTMGL1.003/2000.02.11/$filename.SRTMGL1.hgt.zip" -O "srtm/$filename.hgt.zip"
    unzip "srtm/$filename.hgt.zip" -d srtm
fi