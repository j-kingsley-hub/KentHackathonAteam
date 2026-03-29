from PIL import Image
import sys
import os

def convert_image(input_file, output_file, var_name, size=(160, 160)):
    try:
        img = Image.open(input_file).convert("RGBA")
        img = img.resize(size, Image.Resampling.LANCZOS)
        w, h = img.size
        
        out_data = []
        for y in range(h):
            for x in range(w):
                r, g, b, a = img.getpixel((x, y))
                r5 = (r >> 3) & 0x1F
                g6 = (g >> 2) & 0x3F
                b5 = (b >> 3) & 0x1F
                rgb565 = (r5 << 11) | (g6 << 5) | b5
                out_data.append(rgb565 & 0xFF)
                out_data.append((rgb565 >> 8) & 0xFF)
                out_data.append(a)

        with open(output_file, 'w') as f:
            f.write("#include \"lv_port.h\"\n\n")
            f.write("#ifndef LV_ATTRIBUTE_MEM_ALIGN\n")
            f.write("#define LV_ATTRIBUTE_MEM_ALIGN\n")
            f.write("#endif\n\n")
            f.write(f"const LV_ATTRIBUTE_MEM_ALIGN uint8_t {var_name}_data[] = {{\n")
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
        print(f"Created {output_file}")
    except Exception as e:
        print(f"Failed to process {input_file}: {e}")

convert_image('main/img/DinoTest2.jpg', 'main/dino_thumb.c', 'dino_thumb_img')
convert_image('main/img/CaveManTest.jpg', 'main/caveman_thumb.c', 'caveman_thumb_img')
convert_image('main/img/MammothTest.jpg', 'main/mammoth_thumb.c', 'mammoth_thumb_img')
convert_image('main/img/DinoTest3.jpg', 'main/dino3_thumb.c', 'dino3_thumb_img')
