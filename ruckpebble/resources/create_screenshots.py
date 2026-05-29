#!/usr/bin/env python3
import argparse
import re
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


SCREEN_LAYOUTS = {
    "profile": {
        "canvas_size": (1500, 920),
        "sections": {
            "Profiles": {"anchor": (34, 86), "box": (50, 175, 405), "side": "left"},
        },
        "footer_box": (1030, 20, 1490, 190),
    },
    "tracking": {
        "canvas_size": (1500, 920),
        "sections": {
            "Active Profile": {"anchor": (34, 18), "box": (50, 175, 405), "side": "left"},
            "Session Pace": {"anchor": (42, 56), "box": (50, 338, 430), "side": "left"},
            "Current Pace": {"anchor": (60, 100), "box": (50, 500, 430), "side": "left"},
            "Steps": {"anchor": (58, 188), "box": (50, 675, 405), "side": "left"},
            "Watch Time": {"anchor": (172, 20), "box": (1045, 175, 405), "side": "right"},
            "Distance": {"anchor": (164, 56), "box": (1045, 338, 405), "side": "right"},
            "Elapsed Time": {"anchor": (154, 104), "box": (1045, 500, 405), "side": "right"},
            "Heart Rate": {"anchor": (102, 104), "box": (1045, 620, 405), "side": "right"},
            "Calories": {"anchor": (152, 188), "box": (1045, 760, 405), "side": "right"},
        },
        "footer_box": (910, 20, 1490, 190),
    },
    "settings": {
        "canvas_size": (1800, 1500),
        "mock_box": (620, 70, 560, 1260),
        "sections": {
            "Shared": {"anchor": (32, 110), "box": (50, 250, 460), "side": "left"},
            "Profile 1": {"anchor": (32, 270), "box": (50, 445, 460), "side": "left"},
            "Profile 2": {"anchor": (32, 468), "box": (1290, 350, 460), "side": "right"},
            "Profile 3": {"anchor": (32, 666), "box": (50, 670, 460), "side": "left"},
            "Tracked Totals": {"anchor": (32, 864), "box": (1290, 680, 460), "side": "right"},
            "Last Activity": {"anchor": (32, 1000), "box": (1290, 890, 460), "side": "right"},
        },
        "footer_box": (1270, 20, 1770, 170),
    },
}


def load_font(size, bold=False):
    candidates = []
    if bold:
        candidates.extend([
            "/System/Library/Fonts/Supplemental/Arial Bold.ttf",
            "/System/Library/Fonts/Supplemental/Helvetica Bold.ttf",
        ])
    candidates.extend([
        "/System/Library/Fonts/Supplemental/Arial.ttf",
        "/System/Library/Fonts/Supplemental/Helvetica.ttf",
        "/Library/Fonts/Arial.ttf",
    ])
    for path in candidates:
        try:
            return ImageFont.truetype(path, size)
        except OSError:
            continue
    return ImageFont.load_default()


def parse_documents(path):
    text = path.read_text()
    blocks = [block.strip() for block in re.split(r"(?m)^\s*---\s*$", text) if block.strip()]
    docs = []
    for block in blocks:
        docs.append(parse_document(block))
    return docs


def parse_document(text):
    doc = {"title": "", "sections": {}}
    current_section = None
    current_key = None
    for raw_line in text.splitlines():
        line = raw_line.strip()
        if not line:
            continue
        if line.startswith("# "):
            if not doc["title"]:
                doc["title"] = line[2:].strip()
                continue
            current_section = line[2:].strip()
            doc["sections"].setdefault(current_section, {})
            current_key = None
            continue
        if line.startswith("## "):
            current_section = line[3:].strip()
            doc["sections"].setdefault(current_section, {})
            current_key = None
            continue
        m = re.match(r"^-\s*([A-Za-z0-9_]+):\s*(.*)$", line)
        if m and current_section:
            key = m.group(1).lower()
            value = m.group(2).strip()
            doc["sections"][current_section][key] = value
            current_key = key
            continue
        if current_section and current_key:
            doc["sections"][current_section][current_key] += "\n" + line
    return doc


def wrap_text(draw, text, font, width):
    lines = []
    if not text:
        return lines
    for paragraph in text.splitlines():
        if not paragraph.strip():
            lines.append("")
            continue
        words = paragraph.split()
        current = ""
        for word in words:
            candidate = f"{current} {word}".strip()
            if draw.textlength(candidate, font=font) <= width:
                current = candidate
            else:
                if current:
                    lines.append(current)
                current = word
        if current:
            lines.append(current)
    return lines


def render_callout(draw, title_font, body_font, box, title, body, fill="#ffffff", outline="#d7dbe0", title_color="#111111", body_color="#444444"):
    bx, by, bw = box
    padding = 16
    lines = wrap_text(draw, body, body_font, bw - padding * 2)
    height = padding * 2 + 30 + len(lines) * 24
    draw.rounded_rectangle((bx, by, bx + bw, by + height), radius=10, fill=fill, outline=outline, width=2)
    draw.text((bx + padding, by + 12), title, fill=title_color, font=title_font)
    yy = by + 48
    for line in lines:
        draw.text((bx + padding, yy), line, fill=body_color, font=body_font)
        yy += 24
    return height


def draw_link(draw, start, end, color="#333333"):
    draw.line((start, end), fill=color, width=2)


def draw_anchor(draw, point, accent="#e45545"):
    r = 6
    x, y = point
    draw.ellipse((x - r, y - r, x + r, y + r), fill=accent, outline="white", width=2)


def place_watch(canvas, screenshot, scale=1.0, top=150):
    watch = Image.open(screenshot).convert("RGB")
    watch = watch.resize((int(watch.width * scale), int(watch.height * scale)), Image.Resampling.NEAREST)
    wx = (canvas.width - watch.width) // 2
    shadow = Image.new("RGBA", (watch.width + 24, watch.height + 24), (0, 0, 0, 0))
    shadow_draw = ImageDraw.Draw(shadow)
    shadow_draw.rounded_rectangle((12, 12, watch.width + 12, watch.height + 12), radius=26, fill=(0, 0, 0, 55))
    canvas.paste(shadow, (wx - 12, top - 12), shadow)
    canvas_draw = ImageDraw.Draw(canvas)
    canvas_draw.rounded_rectangle((wx - 8, top - 8, wx + watch.width + 8, top + watch.height + 8), radius=18, fill="#111111")
    canvas.paste(watch, (wx, top))
    return wx, top, watch.width, watch.height


def draw_mock_card(draw, box, title, lines, fill="#ffffff", outline="#d7dbe0"):
    x, y, w, h = box
    draw.rounded_rectangle((x, y, x + w, y + h), radius=10, fill=fill, outline=outline, width=2)
    title_font = load_font(24, True)
    body_font = load_font(18)
    draw.text((x + 16, y + 12), title, fill="#111111", font=title_font)
    yy = y + 48
    for label, value in lines:
        draw.text((x + 16, yy), label, fill="#555555", font=body_font)
        draw.rounded_rectangle((x + 170, yy - 2, x + w - 16, yy + 24), radius=6, fill="#f7f7f7", outline="#e2e2e2", width=1)
        draw.text((x + 182, yy), value, fill="#111111", font=body_font)
        yy += 34


def place_settings_mock(canvas, layout):
    x, y, w, h = layout["mock_box"]
    page = Image.new("RGB", (w, h), "#eaf3ff")
    draw = ImageDraw.Draw(page)
    title_font = load_font(30, True)
    subtitle_font = load_font(18)
    draw.text((18, 16), "Ruck Settings", fill="#111111", font=title_font)
    draw.text((18, 52), "Shared watch settings, the three ruck profiles, totals, and the last activity summary", fill="#555555", font=subtitle_font)

    draw_mock_card(draw, (16, 88, 528, 150), "Shared", [
        ("Body weight", "81.5"),
        ("Ruck weight unit", "lb"),
        ("Stride length", "79.0"),
    ])
    draw_mock_card(draw, (16, 252, 528, 180), "Profile 1", [
        ("Profile name", "30lb, road"),
        ("Ruck weight", "30.0"),
        ("Terrain", "Road"),
        ("Grade (%)", "0"),
    ])
    draw_mock_card(draw, (16, 448, 528, 180), "Profile 2", [
        ("Profile name", "15lb, trail, hilly"),
        ("Ruck weight", "15.0"),
        ("Terrain", "Gravel"),
        ("Grade (%)", "10"),
    ])
    draw_mock_card(draw, (16, 644, 528, 180), "Profile 3", [
        ("Profile name", ""),
        ("Ruck weight", "30.0"),
        ("Terrain", "Mixed"),
        ("Grade (%)", "0"),
    ])
    draw_mock_card(draw, (16, 840, 528, 132), "Tracked Totals", [
        ("Lifetime distance (km)", "--"),
        ("Lifetime calories", "--"),
    ])
    draw_mock_card(draw, (16, 988, 528, 154), "Last Activity", [
        ("Date / Time", "--"),
        ("Distance (km)", "--"),
        ("Pace", "--"),
        ("Calories", "--"),
    ])
    draw.rounded_rectangle((16, 1156, 528, 1214), radius=10, fill="#111111", outline="#111111", width=2)
    draw.text((36, 1173), "Save", fill="#ffffff", font=load_font(22, True))
    draw.rounded_rectangle((344, 1156, 528, 1214), radius=10, fill="#666666", outline="#666666", width=2)
    draw.text((381, 1173), "Reset", fill="#ffffff", font=load_font(22, True))

    canvas.paste(page, (x, y))
    return x, y, w, h


def render_screen(doc, screenshot_path, output_path):
    title = doc["title"].lower()
    if "profile" in title:
        screen_key = "profile"
    elif "tracking" in title:
        screen_key = "tracking"
    elif "settings" in title:
        screen_key = "settings"
    else:
        screen_key = "tracking"

    layout = SCREEN_LAYOUTS[screen_key]
    canvas = Image.new("RGB", layout.get("canvas_size", (1500, 1000)), "#f6f7f8")
    draw = ImageDraw.Draw(canvas)

    f_title = load_font(44, True)
    f_subtitle = load_font(20)
    f_box_title = load_font(25, True)
    f_box_body = load_font(20)
    f_footer = load_font(15)

    header = doc["sections"].get("Header", {})
    draw.text((56, 40), header.get("title", doc["title"]), fill="#111111", font=f_title)
    header_subtitle = header.get("subtitle", "")
    if header_subtitle:
        header_lines = wrap_text(draw, header_subtitle, f_subtitle, canvas.width - 116)
        yy = 92
        for line in header_lines:
            draw.text((58, yy), line, fill="#555555", font=f_subtitle)
            yy += 24

    if screen_key == "settings":
        wx, wy, watch_w, watch_h = place_settings_mock(canvas, layout)

        def to_canvas_point(point):
            return (wx + int(point[0]) - 20, wy + int(point[1]))
    else:
        wx, wy, watch_w, watch_h = place_watch(canvas, screenshot_path)

        def to_canvas_point(point):
            return (wx + int(point[0]), wy + int(point[1]))

    for section_name, section_layout in layout["sections"].items():
        if screen_key == "settings" and section_name == "Header":
            continue
        section = doc["sections"].get(section_name, {})
        body = section.get("description", "")
        if not body and section_name == "Header":
            body = section.get("subtitle", "")
        if not body:
            continue
        bx, by, bw = section_layout["box"]
        height = render_callout(draw, f_box_title, f_box_body, (bx, by, bw), section_name, body)
        ax, ay = to_canvas_point(section_layout["anchor"])
        if section_layout["side"] == "left":
            start = (bx + bw, by + max(30, height - 18))
        else:
            start = (bx, by + max(30, height - 18))
        draw_link(draw, start, (ax, ay))
        draw_anchor(draw, (ax, ay))

    footer = doc["sections"].get("Footer", {})
    footer_box = layout["footer_box"]
    bx, by, bw, bh = footer_box
    footer_subtitle = footer.get("subtitle", "")
    footer_lines = wrap_text(draw, footer_subtitle, f_footer, bw - 60) if footer_subtitle else []
    footer_height = 14 + 24 + max(1, len(footer_lines)) * 20
    footer_box = (bx, by, bw, min(bh, by + footer_height))
    draw.rounded_rectangle(footer_box, radius=12, fill="#e6f0ff", outline="#8ab2e6", width=2)
    draw.text((bx + 24, by + 10), footer.get("title", ""), fill="#1f4f84", font=f_box_title)
    if footer_lines:
        yy = by + 38
        for line in footer_lines:
            draw.text((bx + 24, yy), line, fill="#1f4f84", font=f_footer)
            yy += 20

    output_path.parent.mkdir(parents=True, exist_ok=True)
    canvas.save(output_path)
    return output_path


def output_name_for_doc(doc):
    title = doc["title"].lower()
    if "profile" in title:
        return "ruck_profile_screen_annotated.png"
    if "tracking" in title:
        return "ruck_tracking_screen_annotated.png"
    if "settings" in title:
        return "ruck_settings_page_annotated.png"
    slug = re.sub(r"[^a-z0-9]+", "_", title).strip("_") or "screen"
    return f"{slug}_annotated.png"


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--spec", default=str(Path(__file__).with_name("screenshot_descriptions.md")))
    parser.add_argument("--profile-screenshot", default="/tmp/ruck_profile.png")
    parser.add_argument("--tracking-screenshot", default="/tmp/ruck_tracking_raw.png")
    parser.add_argument("--settings-screenshot", default=None)
    parser.add_argument("--output-dir", default=str(Path(__file__).with_name("explainer_screenshots")))
    args = parser.parse_args()

    docs = parse_documents(Path(args.spec))
    outputs = []

    for doc in docs:
        title = doc["title"].lower()
        if "profile" in title:
            screenshot = Path(args.profile_screenshot)
        elif "tracking" in title:
            screenshot = Path(args.tracking_screenshot)
        elif "settings" in title:
            screenshot = Path(args.settings_screenshot) if args.settings_screenshot else None
        else:
            continue
        output = Path(args.output_dir) / output_name_for_doc(doc)
        outputs.append(render_screen(doc, screenshot, output))

    for output in outputs:
        print(output)


if __name__ == "__main__":
    main()
