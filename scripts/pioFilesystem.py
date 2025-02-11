Import("env")

import csv
import os
import subprocess
from os import path

def convertToBytes(size_str):
    # Remove spaces and convert to uppercase
    size_str = size_str.replace(" ", "").strip().upper()
    # Kebibytes
    if size_str.endswith('K'):
        return int(size_str[:-1]) * 1024
    # Mebibytes
    elif size_str.endswith('M'):
        return int(size_str[:-1]) * 1024 * 1024
    # Hexadecimal
    elif size_str.startswith('0X'):
        return int(size_str, 16)
    else:
        return int(size_str)


def createLittlefsImage(imageName, imageSize, directory):

    mklittlefs = path.join(env.PioPlatform().get_package_dir("tool-mklittlefs"), "mklittlefs")

    command = [
        mklittlefs,
        "-c", str(directory),
        "-d", str((imageSize // 4096)),
        "-s", str(imageSize),
        str(imageName)
    ]

    try:
        result = subprocess.run(command, check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    except subprocess.CalledProcessError as e:
        print("Error creating LittleFS image:")
        print(e.stderr.decode())
        env.Exit(1)


def getMostRecentlyUpdatedFile(directory):
    most_recent_time = 0

    for root, _, files in os.walk(directory):
        for file in files:
            file_path = os.path.join(root, file)
            file_mtime = os.path.getmtime(file_path)
            if file_mtime > most_recent_time:
                most_recent_time = file_mtime
    return most_recent_time


def getFileTimestamp(file_path):
    if os.path.isfile(file_path):
        file_mtime = os.path.getmtime(file_path)
        return file_mtime


buildDir = env.subst("$BUILD_DIR")
projectDir = env.subst("$PROJECT_DIR")
staticAssetsTableName = "static_assets"
staticAssetsPath = projectDir + "/filesystem/static_assets"
staticAssetsBinPath = buildDir + "/" + staticAssetsTableName + ".bin"
otaAssetsTableName = "ota_a_assets"
otaAssetsPath = projectDir + "/filesystem/ota_assets"
otaAssetsBinPath = buildDir + "/" + otaAssetsTableName + ".bin"

with open(projectDir + "/partitions.csv", mode='r') as file:
    csv_reader = csv.reader(file)
    # Look for the static assets and ota assets partitions
    for row in csv_reader:
        if row[0] == staticAssetsTableName:
            staticAssetsSize = convertToBytes(row[4])
            staticAssetsOffset = convertToBytes(row[3])
        if row[0] == otaAssetsTableName:
            otaAssetsSize = convertToBytes(row[4])
            otaAssetsOffset = convertToBytes(row[3])

# Only create the image if it doesn't exist or if the files have been updated
if (not os.path.isfile(staticAssetsBinPath) or getFileTimestamp(staticAssetsBinPath) < getMostRecentlyUpdatedFile(
        staticAssetsPath)):
    createLittlefsImage(imageName=staticAssetsBinPath, imageSize=staticAssetsSize,
                        directory=staticAssetsPath)
    # Mark the image to be included in the flashing process
    env.Append(FLASH_EXTRA_IMAGES=[(hex(staticAssetsOffset), staticAssetsBinPath)])

# Only create the image if it doesn't exist or if the files have been updated
if (not os.path.isfile(otaAssetsBinPath) or getFileTimestamp(otaAssetsBinPath) < getMostRecentlyUpdatedFile(
        otaAssetsPath)):
    createLittlefsImage(imageName=otaAssetsBinPath, imageSize=otaAssetsSize,
                        directory=otaAssetsPath)
    # Mark the image to be included in the flashing process
    env.Append(FLASH_EXTRA_IMAGES=[(hex(otaAssetsOffset), otaAssetsBinPath)])
