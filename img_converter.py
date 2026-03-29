from PIL import Image
import sys
import os

def convert_image(input_file, output_file, var_name):
    img = Image.open(input_file).convert("RGBA")
    # if width > 256 or height > 256, just resize it.
    w, h = img.size
    
    out_data = []
    
    for y in range(h):
        for x in range(w):
            r, g, b, a = img.getpixel((x, y))
            # RGB565
            r5 = (r >> 3) & 0x1F
            g6 = (g >> 2) & 0x3F
            b5 = (b >> 3) & 0x1F
            
            rgb565 = (r5 << 11) | (g6 << 5) | b5
            
            # LVGL True Color Alpha for 16-bit depth:
            # lower byte of rgb565, upper byte of rgb565, alpha
            out_data.append(rgb565 & 0xFF)
            out_data.append((rgb565 >> 8) & 0xFF)
            out_data.append(a)

    with open(output_file, 'w') as f:
        f.write("#include \"lv_port.h\"\n\n")
        f.write("#ifndef LV_ATTRIBUTE_MEM_ALIGN\n")
        f.write("#define LV_ATTRIBUTE_MEM_ALIGN\n")
        f.write("#endif\n\n")
        f.write(f"const LV_ATTRIBUTE_MEM_ALIGN uint8_t {var_name}_data[] = {{\n")
        
        # Write bytes in rows of 16
        for i in range(0, len(out_data), 16):
            chunk = out_data[i:i+16]
            f.write("  " + ",".join([f"0x{b:02x}" for b in chunk]) + ",\n")
            
        f.write("};\n\n")
        
        f.write(f"const lv_img_dsc_t {var_name} = {{\n")
        f.write("  .header.always_zero = 0,\n")
        f.write(f"  .header.w = {w},\n")
        f.write(f"  .header.h = {h},\n")
        f.write(f"  .data_size = sizeof({var_name}_data),\n")
        f.write("  .header.cf = LV_IMG_CF_TRUE_COLOR_ALPHA,\n")
        f.write(f"  .data = {var_name}_data}};\n")

convert_image('temp_main/img/dogprototype.png', 'main/dogprototype_img.c', 'dogprototype_img')
convert_image('temp_main/img/dogprototype3.png', 'main/dogprototype3_img.c', 'dogprototype3_img')

# For dogTalking.jpg, it's 1081x1043. We should resize it to ~200x200 or something to fit the screen.
img = Image.open('temp_main/img/dogTalking.jpg')
img = img.resize((200, 200), Image.Resampling.LANCZOS)
img.save('temp_main/img/dogTalking_resized.png')
convert_image('temp_main/img/dogTalking_resized.png', 'main/dogTalking_img.c', 'dogTalking_img')

print("Done")
