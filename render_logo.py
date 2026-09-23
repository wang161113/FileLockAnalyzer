from PIL import Image, ImageDraw, ImageFont
import os

CANVAS = 256
MARGIN = 18

FOLDER_TAB_W = int(CANVAS * 0.32)
FOLDER_TAB_H = int(CANVAS * 0.12)
FOLDER_TAB_X = MARGIN + int(CANVAS * 0.03)
FOLDER_BODY_R = 22

C_BODY_TOP = (26, 93, 176, 255)      # 稍亮的钢蓝（文件盖上表面）
C_BODY_BOT = (12, 48, 118, 255)      # 深钢蓝（文件盖下半）
C_BACK       = (47, 163, 138, 255)   # 铜绿（后盖 tab，与之前 ring 色一致）
C_RIM        = (232, 241, 255, 230)  # 冷白 rim light
C_ZHAN_CN    = (255, 255, 255, 255)  # 「占」字纯白

# Default fallback fonts (Windows) — will pick first hit from the ordered list.
FONT_CANDIDATES = [
    r"C:\Windows\Fonts\msyhbd.ttc",       # Microsoft YaHei Bold
    r"C:\Windows\Fonts\msyh.ttc",         # Microsoft YaHei
    r"C:\Windows\Fonts\simhei.ttf",       # SimHei
    r"C:\Windows\Fonts\simsun.ttc",       # SimSun
]


def _load_font(size):
    for p in FONT_CANDIDATES:
        try:
            return ImageFont.truetype(p, size=size)
        except Exception:
            continue
    return ImageFont.load_default()


def _draw_folder_shape(draw):
    # 后盖 tab（突出于主体左上角，偏上 1/4）
    tab_x0 = FOLDER_TAB_X
    tab_y0 = MARGIN + int(FOLDER_TAB_H * 0.25)
    tab_x1 = tab_x0 + FOLDER_TAB_W
    tab_y1 = tab_y0 + FOLDER_TAB_H
    draw.rounded_rectangle([tab_x0, tab_y0, tab_x1, tab_y1 + int(FOLDER_TAB_H * 0.4)],
                           radius=10, fill=C_BACK)

    # 主体圆角矩形（盖住 tab 下半部分，形成文件夹层级关系）
    body_x0 = MARGIN
    body_y0 = MARGIN + FOLDER_TAB_H
    body_x1 = CANVAS - MARGIN
    body_y1 = CANVAS - MARGIN
    draw.rounded_rectangle([body_x0, body_y0, body_x1, body_y1],
                           radius=FOLDER_BODY_R, fill=C_BODY_BOT)
    # 顶部更亮的盖面（横向约 38% 高度，形成渐变视觉）
    cover_h = int((body_y1 - body_y0) * 0.40)
    # 只画圆角上半，与主体下半拼合
    cover = Image.new("RGBA", (CANVAS, CANVAS), (0, 0, 0, 0))
    cd = ImageDraw.Draw(cover)
    cd.rounded_rectangle([body_x0, body_y0, body_x1, body_y0 + cover_h + FOLDER_BODY_R],
                         radius=FOLDER_BODY_R, fill=C_BODY_TOP)
    # 裁掉底部超出的圆角（只保留上半盖面）
    mask = Image.new("L", (CANVAS, CANVAS), 0)
    md = ImageDraw.Draw(mask)
    md.rectangle([body_x0, body_y0, body_x1, body_y0 + cover_h], fill=255)
    return (body_x0, body_y0, body_x1, body_y1, cover, mask)


def main():
    img = Image.new("RGBA", (CANVAS, CANVAS), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)

    body = _draw_folder_shape(draw)
    bx0, by0, bx1, by1, cover, mask = body
    img = Image.alpha_composite(img, Image.composite(cover, Image.new("RGBA", (CANVAS, CANVAS), (0, 0, 0, 0)), mask))
    draw = ImageDraw.Draw(img)

    # 外围 rim light（1px 冷白，外缩 1px）
    draw.rounded_rectangle([bx0 + 2, by0 + 2, bx1 - 2, by1 - 2],
                           radius=FOLDER_BODY_R - 1, outline=C_RIM, width=1)

    # 主体内部的「占」字 —— 占整个文件夹可视区的 ~62%，绝对居中
    inner_w = bx1 - bx0
    inner_h = by1 - by0
    font_size = int(min(inner_w, inner_h) * 0.70)
    font = _load_font(font_size)
    text = "\u5360"  # 「占」
    # measure text
    bbox = draw.textbbox((0, 0), text, font=font)
    tw = bbox[2] - bbox[0]
    th = bbox[3] - bbox[1]
    tx = bx0 + (inner_w - tw) // 2 - bbox[0]
    ty = by0 + (inner_h - th) // 2 - bbox[1] + int(inner_h * 0.02)
    draw.text((tx, ty), text, fill=C_ZHAN_CN, font=font)

    out_dir = r"c:\Users\Administrator\Documents\trae_projects\Handling File Usage"
    png_path = os.path.join(out_dir, "FileLockAnalyzer_256.png")
    img.save(png_path, "PNG")

    sizes = [(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)]
    imgs = [img.resize(s, Image.LANCZOS) for s in sizes]
    ico_path = os.path.join(out_dir, "FileLockAnalyzer.ico")
    imgs[0].save(ico_path, format="ICO", sizes=sizes, append_images=imgs[1:])

    print("Saved:", png_path)
    print("Saved:", ico_path)


if __name__ == "__main__":
    main()
