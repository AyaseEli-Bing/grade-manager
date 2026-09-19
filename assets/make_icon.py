"""生成应用图标：深蓝底 + 递进柱状图标记（纯几何，无文字无 emoji）。"""

from PIL import Image, ImageDraw

SIZE = 1024
SS = 4  # 超采样倍数，保证圆角与边缘平滑
S = SIZE * SS

BG = (24, 58, 92, 255)        # 深蓝底
BAR = (45, 212, 191, 255)     # 青绿主柱
BAR_2 = (125, 211, 252, 255)  # 浅蓝次柱

img = Image.new("RGBA", (S, S), (0, 0, 0, 0))
d = ImageDraw.Draw(img)

# 圆角底板
d.rounded_rectangle([0, 0, S - 1, S - 1], radius=int(S * 0.22), fill=BG)

# 三组递进柱状图，居中偏下
bar_w = int(S * 0.115)
gap = int(S * 0.075)
heights = [int(S * 0.20), int(S * 0.30), int(S * 0.40)]
total_w = bar_w * 3 + gap * 2
left = (S - total_w) // 2
baseline = int(S * 0.72)

for i, h in enumerate(heights):
    x0 = left + i * (bar_w + gap)
    x1 = x0 + bar_w
    y0 = baseline - h
    color = BAR if i == 2 else BAR_2
    d.rounded_rectangle([x0, y0, x1, baseline], radius=int(bar_w * 0.35), fill=color)

# 基线
d.rounded_rectangle(
    [int(S * 0.22), baseline + int(S * 0.045), int(S * 0.78), baseline + int(S * 0.045) + int(S * 0.022)],
    radius=int(S * 0.011),
    fill=(255, 255, 255, 200),
)

img = img.resize((SIZE, SIZE), Image.LANCZOS)
img.save("/Users/bing1111/WorkBuddy/2026-09-19-11-49-05/grade-manager/assets/icon.png")
print("icon.png written")
