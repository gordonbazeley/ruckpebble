#!/usr/bin/env python3
import argparse
import re
import sys
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
            "Watch Time": {"anchor": (172, 20), "box": (1045, 215, 405), "side": "right"},
            "Distance": {"anchor": (164, 56), "box": (1045, 338, 405), "side": "right"},
            "Elapsed Time": {"anchor": (154, 104), "box": (1045, 500, 405), "side": "right"},
            "Heart Rate": {"anchor": (102, 104), "box": (1045, 620, 405), "side": "right"},
            "Calories": {"anchor": (152, 188), "box": (1045, 760, 405), "side": "right"},
        },
        "footer_box": (910, 20, 1490, 280),
    },
}

# Settings page: each section maps to one or more real screenshots (stacked vertically)
SETTINGS_SECTIONS = [
    {"name": "About you", "files": ["settings_about_you.png"], "side": "right"},
    {"name": "Profiles",  "files": ["settings_profiles.png", "settings_profile_edit.png"], "side": "left"},
    {"name": "Calories",  "files": ["settings_calories.png"],  "side": "right"},
    {"name": "History",   "files": ["settings_history.png"],   "side": "left"},
]


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
    return [parse_document(block) for block in blocks]


def parse_document(text):
    doc = {"title": "", "sections": {}}
    current_section = None
    current_key = None
    value_start_col = 0
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
            colon_idx = raw_line.index(":")
            value_start_col = colon_idx + 2
            continue
        if current_section and current_key:
            raw_indent = len(raw_line) - len(raw_line.lstrip())
            relative_indent = max(0, raw_indent - value_start_col)
            normalized = " " * relative_indent + line
            doc["sections"][current_section][current_key] += "\n" + normalized
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


BULLET = "•"


def layout_lines(draw, text, font, width):
    """Returns list of (display_text, x_offset_px) for rendering.

    Lines starting with '* ' become level-1 bullets; lines starting with
    two or more spaces then '* ' become level-2 bullets (indented further).
    """
    result = []
    if not text:
        return result

    for raw_para in text.splitlines():
        if not raw_para.strip():
            result.append(("", 0))
            continue

        stripped = raw_para.lstrip()
        leading = len(raw_para) - len(stripped)

        m_bullet = re.match(r"^\* (.+)$", stripped)
        if m_bullet:
            bullet_text = m_bullet.group(1)
            indent_px = 20 if leading >= 2 else 0
            prefix = f"{BULLET} "
            prefix_w = int(draw.textlength(prefix, font=font))
            cont_x = indent_px + prefix_w

            words = bullet_text.split()
            current = ""
            first = True
            for word in words:
                candidate = f"{current} {word}".strip()
                avail = width - (indent_px if first else cont_x)
                if draw.textlength(candidate, font=font) <= avail:
                    current = candidate
                else:
                    if current:
                        result.append((prefix + current if first else current, indent_px if first else cont_x))
                        first = False
                    current = word
            if current:
                result.append((prefix + current if first else current, indent_px if first else cont_x))
        else:
            words = stripped.split()
            current = ""
            for word in words:
                candidate = f"{current} {word}".strip()
                if draw.textlength(candidate, font=font) <= width:
                    current = candidate
                else:
                    if current:
                        result.append((current, 0))
                    current = word
            if current:
                result.append((current, 0))

    return result


def render_callout(draw, title_font, body_font, box, title, body, fill="#ffffff", outline="#d7dbe0", title_color="#111111", body_color="#444444"):
    bx, by, bw = box
    padding = 16
    lines = layout_lines(draw, body, body_font, bw - padding * 2)
    height = padding * 2 + 30 + len(lines) * 24
    draw.rounded_rectangle((bx, by, bx + bw, by + height), radius=10, fill=fill, outline=outline, width=2)
    draw.text((bx + padding, by + 12), title, fill=title_color, font=title_font)
    yy = by + 48
    for line_text, x_off in lines:
        draw.text((bx + padding + x_off, yy), line_text, fill=body_color, font=body_font)
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


def place_framed_screenshot(canvas, img, x, y):
    shadow = Image.new("RGBA", (img.width + 20, img.height + 20), (0, 0, 0, 0))
    ImageDraw.Draw(shadow).rounded_rectangle(
        (10, 10, img.width + 10, img.height + 10), radius=12, fill=(0, 0, 0, 40)
    )
    canvas.paste(shadow, (x - 10, y - 10), shadow)
    ImageDraw.Draw(canvas).rounded_rectangle(
        (x - 3, y - 3, x + img.width + 3, y + img.height + 3), radius=8, fill="#cccccc"
    )
    canvas.paste(img, (x, y))


def render_screen(doc, screenshot_path, output_path):
    title = doc["title"].lower()
    screen_key = "tracking" if "tracking" in title else "profile"

    layout = SCREEN_LAYOUTS[screen_key]
    canvas = Image.new("RGB", layout.get("canvas_size", (1500, 1000)), "#f6f7f8")
    draw = ImageDraw.Draw(canvas)

    f_title = load_font(44, True)
    f_subtitle = load_font(20)
    f_box_title = load_font(25, True)
    f_box_body = load_font(20)
    f_footer = f_box_body

    header = doc["sections"].get("Header", {})
    draw.text((56, 40), header.get("title", doc["title"]), fill="#111111", font=f_title)
    header_subtitle = header.get("subtitle", "")
    if header_subtitle:
        header_lines = wrap_text(draw, header_subtitle, f_subtitle, canvas.width - 116)
        yy = 92
        for line in header_lines:
            draw.text((58, yy), line, fill="#555555", font=f_subtitle)
            yy += 24

    wx, wy, watch_w, watch_h = place_watch(canvas, screenshot_path)

    def to_canvas_point(point):
        return (wx + int(point[0]), wy + int(point[1]))

    for section_name, section_layout in layout["sections"].items():
        section = doc["sections"].get(section_name, {})
        body = section.get("description", "")
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
    footer_lines = layout_lines(draw, footer_subtitle, f_footer, bw - bx - 48) if footer_subtitle else []
    footer_height = 14 + 24 + max(1, len(footer_lines)) * 24
    footer_box = (bx, by, bw, min(bh, by + footer_height))
    draw.rounded_rectangle(footer_box, radius=12, fill="#e6f0ff", outline="#8ab2e6", width=2)
    draw.text((bx + 24, by + 10), footer.get("title", ""), fill="#1f4f84", font=f_box_title)
    if footer_lines:
        yy = by + 38
        for line_text, x_off in footer_lines:
            draw.text((bx + 24 + x_off, yy), line_text, fill="#1f4f84", font=f_footer)
            yy += 24

    output_path.parent.mkdir(parents=True, exist_ok=True)
    canvas.save(output_path)
    return output_path


def render_settings_screen(doc, screenshots_dir, output_path):
    SCREENSHOT_W = 420
    CANVAS_W = 1800
    SECTION_GAP = 70
    IMG_GAP = 24
    HEADER_H = 120
    PAD = 40
    CALLOUT_W = 580
    IMG_X = (CANVAS_W - SCREENSHOT_W) // 2  # = 690

    f_title = load_font(44, True)
    f_subtitle = load_font(20)
    f_box_title = load_font(25, True)
    f_box_body = load_font(20)

    screenshots_dir = Path(screenshots_dir)

    def load_img(filename):
        p = screenshots_dir / filename
        if not p.exists():
            return None
        img = Image.open(p).convert("RGB")
        return img.resize((SCREENSHOT_W, int(img.height * SCREENSHOT_W / img.width)), Image.Resampling.LANCZOS)

    section_imgs = {}
    for s in SETTINGS_SECTIONS:
        imgs = []
        for f in s["files"]:
            img = load_img(f)
            if img:
                imgs.append(img)
        section_imgs[s["name"]] = imgs

    # Compute section y positions from screenshot heights
    section_ys = {}
    y = HEADER_H
    for s in SETTINGS_SECTIONS:
        imgs = section_imgs[s["name"]]
        if not imgs:
            continue
        section_ys[s["name"]] = y
        total_img_h = sum(i.height for i in imgs) + IMG_GAP * (len(imgs) - 1)
        y += total_img_h + SECTION_GAP

    canvas = Image.new("RGB", (CANVAS_W, y + PAD), "#f6f7f8")
    draw = ImageDraw.Draw(canvas)

    # Header
    header = doc["sections"].get("Header", {})
    draw.text((56, 40), header.get("title", doc["title"]), fill="#111111", font=f_title)
    subtitle = header.get("subtitle", "")
    if subtitle:
        yy = 92
        for line in wrap_text(draw, subtitle, f_subtitle, CANVAS_W - 116):
            draw.text((58, yy), line, fill="#555555", font=f_subtitle)
            yy += 24

    LEFT_CALLOUT_X = 20
    RIGHT_CALLOUT_X = CANVAS_W - 20 - CALLOUT_W  # = 1200

    for s in SETTINGS_SECTIONS:
        name = s["name"]
        imgs = section_imgs.get(name, [])
        if not imgs:
            continue

        sec_y = section_ys[name]
        total_img_h = sum(i.height for i in imgs) + IMG_GAP * (len(imgs) - 1)

        # Draw screenshots stacked vertically
        iy = sec_y
        for img in imgs:
            place_framed_screenshot(canvas, img, IMG_X, iy)
            iy += img.height + IMG_GAP

        # Anchor at vertical midpoint of the screenshot group
        mid_img_y = sec_y + total_img_h // 2
        if s["side"] == "left":
            callout_x = LEFT_CALLOUT_X
            anchor = (IMG_X, mid_img_y)
        else:
            callout_x = RIGHT_CALLOUT_X
            anchor = (IMG_X + SCREENSHOT_W, mid_img_y)

        body = doc["sections"].get(name, {}).get("description", "")
        if body:
            callout_h = render_callout(draw, f_box_title, f_box_body,
                                       (callout_x, sec_y, CALLOUT_W), name, body)
            mid_callout_y = sec_y + callout_h // 2
            if s["side"] == "left":
                link_start = (callout_x + CALLOUT_W, mid_callout_y)
            else:
                link_start = (callout_x, mid_callout_y)
            draw_link(draw, link_start, anchor)

        draw_anchor(draw, anchor)

    output_path.parent.mkdir(parents=True, exist_ok=True)
    canvas.save(output_path)
    return output_path


def output_name_for_doc(doc):
    title = doc["title"].lower()
    if "tracking" in title:
        return "ruck_tracking_screen_annotated.png"
    if "profile" in title and "javascript" not in title and "setting" not in title:
        return "ruck_profile_screen_annotated.png"
    if "setting" in title or "javascript" in title:
        return "ruck_settings_page_annotated.png"
    slug = re.sub(r"[^a-z0-9]+", "_", title).strip("_") or "screen"
    return f"{slug}_annotated.png"


def main():
    default_screenshots = str(Path(__file__).with_name("screenshots"))
    parser = argparse.ArgumentParser()
    parser.add_argument("--spec", default=str(Path(__file__).with_name("screenshot_descriptions.md")))
    parser.add_argument("--screenshots-dir", default=default_screenshots,
                        help="Directory containing profile.jpeg, rucking.jpeg, and settings_*.png")
    parser.add_argument("--output-dir", default=str(Path(__file__).with_name("explainer_screenshots")))
    args = parser.parse_args()

    docs = parse_documents(Path(args.spec))
    screenshots_dir = Path(args.screenshots_dir)
    outputs = []

    for doc in docs:
        title = doc["title"].lower()
        output = Path(args.output_dir) / output_name_for_doc(doc)

        if "setting" in title or "javascript" in title:
            outputs.append(render_settings_screen(doc, screenshots_dir, output))
        elif "tracking" in title:
            screenshot = screenshots_dir / "rucking.jpeg"
            if not screenshot.exists():
                print(f"Skipping '{doc['title']}': {screenshot} not found", file=sys.stderr)
                continue
            outputs.append(render_screen(doc, screenshot, output))
        elif "profile" in title:
            screenshot = screenshots_dir / "profile.jpeg"
            if not screenshot.exists():
                print(f"Skipping '{doc['title']}': {screenshot} not found", file=sys.stderr)
                continue
            outputs.append(render_screen(doc, screenshot, output))

    for output in outputs:
        if output:
            print(output)


if __name__ == "__main__":
    main()
