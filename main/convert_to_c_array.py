import sys
import os
from PIL import Image

def main():
    if len(sys.argv) < 3:
        print("Usage: convert_to_c_array.py <input.webp> <output.c> <array_name>")
        sys.exit(1)
        
    input_file = sys.argv[1]
    output_file = sys.argv[2]
    array_name = sys.argv[3]
    
    # 1. Convert to JPG
    temp_jpg = input_file + ".temp.jpg"
    with Image.open(input_file) as im:
        if im.mode != "RGB":
            im = im.convert("RGB")
        im.save(temp_jpg, "JPEG", quality=60) # Reduced quality for smaller file size
        
    # 2. Read bytes
    with open(temp_jpg, "rb") as f:
        data = f.read()
        
    # 3. Write C array
    with open(output_file, "w") as f:
        f.write(f"#include <stdint.h>\n#include <stddef.h>\n\n")
        f.write(f"const uint8_t {array_name}[] = {{\n")
        
        for i, byte in enumerate(data):
            if i % 12 == 0:
                f.write("    ")
            f.write(f"0x{byte:02x}, ")
            if i % 12 == 11:
                f.write("\n")
                
        f.write(f"\n}};\n\n")
        f.write(f"const size_t {array_name}_size = sizeof({array_name});\n")
        
    # 4. Cleanup
    os.remove(temp_jpg)
    print(f"Created {output_file} successfully.")

if __name__ == "__main__":
    main()
