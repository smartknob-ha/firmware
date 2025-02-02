import struct, sys, semver, os, hashlib


def validate_file(file_path):
    return os.path.isfile(file_path)


def print_error(*args, **kwargs):
    print(*args, file=sys.stderr, **kwargs)


def calculate_sha256(file_contents):
    if file_contents is None:
        return bytes(32)
    sha256_hash = hashlib.sha256()  # Create a SHA-256 hash object
    sha256_hash.update(file_contents)  # Hash the in-memory contents
    print(f"SHA256: {sha256_hash.hexdigest()}")
    return sha256_hash.digest()  # Return the raw 32-byte binary hash


def main():
    print(os.getcwd())
    if len(sys.argv) < 5:
        print_error("Invalid number of arguments")
        print(
            "Usage: python generateOtaBinary.py <Semantic version> <Output path> <App bin path> <OTA assets bin path> <(Optional) Static assets bin path>")
        sys.exit(1)

    try:
        firmwareVersion = semver.VersionInfo.parse(sys.argv[1])
    except ValueError:
        print_error("Invalid semantic version: " + sys.argv[1])
        sys.exit(1)

    outputFilePath = sys.argv[2]
    appFilePath = sys.argv[3]
    if not validate_file(appFilePath):
        print_error("App binary does not exist: " + appFilePath)
        sys.exit(1)
    otaAssetsFilePath = sys.argv[4]
    if not validate_file(otaAssetsFilePath):
        print_error("OTA assets binary does not exist: " + otaAssetsFilePath)
        sys.exit(1)
    staticAssetsFilePath = sys.argv[5] if len(sys.argv) == 6 else None
    if staticAssetsFilePath and not validate_file(staticAssetsFilePath):
        print_error("Static binary does not exist: " + staticAssetsFilePath)
        sys.exit(1)

    # Read the binaries
    with open(appFilePath, 'rb') as appFile:
        appData = appFile.read()
    with open(otaAssetsFilePath, 'rb') as otaAssetsFile:
        otaAssetsData = otaAssetsFile.read()
    appSize = len(appData)
    otaAssetsSize = len(otaAssetsData)
    if staticAssetsFilePath:
        with open(staticAssetsFilePath, 'rb') as staticAssetsFile:
            staticAssetsData = staticAssetsFile.read()
        hasStaticAssets = True
        staticAssetsSize = len(staticAssetsData)
    else:
        staticAssetsData = None
        hasStaticAssets = False
        staticAssetsSize = 0

    # Generate the following struct:
    # struct OtaInfoHeader {
    #     size_t  appSize;
    #     uint8_t appSha[32];
    #     size_t  otaAssetsSize;
    #     uint8_t otaAssetsSha[32];
    #     bool    hasStaticAssets;
    #     size_t  staticAssetsSize;
    #     uint8_t staticAssetsSha[32];
    #     char    firmwareVersion[20];
    # };
    header = struct.pack('I32sI32s?I32s20s', appSize, calculate_sha256(appData), otaAssetsSize, calculate_sha256(otaAssetsData), hasStaticAssets, staticAssetsSize, calculate_sha256(staticAssetsData), str(firmwareVersion).encode('utf-8'))

    print(f"Header size: {len(header)}")

    print(f"App size: {appSize}")
    # print(f"App SHA256: {calculate_sha256(appData)}")
    print(f"OTA assets size: {otaAssetsSize}")
    # print(f"OTA assets SHA256: {calculate_sha256(otaAssetsData)}")
    print(f"Static assets size: {staticAssetsSize}")
    # print(f"Static assets SHA256: {calculate_sha256(staticAssetsData)}")
    print(f"Firmware version: {firmwareVersion}")


    # Write the merged binary
    with open(outputFilePath, 'wb') as output_file:
        output_file.write(header)
        output_file.write(appData)
        output_file.write(otaAssetsData)
        if hasStaticAssets:
            output_file.write(staticAssetsData)

    print(f"Generated merged binary at {outputFilePath}")


if __name__ == "__main__":
    main()
