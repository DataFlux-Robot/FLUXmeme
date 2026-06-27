#!/usr/bin/env python3
"""FLUXmeme visualization tool — render a .flux file's structure.

Shows:
  1. Record graph (nodes = records, edges = links/knowledge graph)
  2. Layer breakdown (BODY / MIND / JOURNAL bar chart)
  3. 3D mesh rendering (if BODY meshes exist)

Usage:
  python demo/flux_viz.py robot.flux              # interactive window
  python demo/flux_viz.py robot.flux --save viz.png  # save to file

Requires: pip install matplotlib numpy
"""
import sys
import os

def main():
    if len(sys.argv) < 2:
        print("Usage: python flux_viz.py <file.flux> [--save out.png]")
        sys.exit(1)

    flux_path = sys.argv[1]
    save_path = None
    if "--save" in sys.argv:
        idx = sys.argv.index("--save")
        save_path = sys.argv[idx + 1] if idx + 1 < len(sys.argv) else "flux_viz.png"

    try:
        import matplotlib
        if save_path:
            matplotlib.use("Agg")
        import matplotlib.pyplot as plt
        import numpy as np
    except ImportError:
        print("Install matplotlib: pip install matplotlib numpy")
        sys.exit(1)

    from fluxmeme import Store, LAYER_BODY, LAYER_MIND, LAYER_JOURNAL

    LAYER_NAMES = {LAYER_BODY: "BODY", LAYER_MIND: "MIND", LAYER_JOURNAL: "JOURNAL"}
    LAYER_COLORS = {LAYER_BODY: "#4A90D9", LAYER_MIND: "#7B68EE", LAYER_JOURNAL: "#FF8C00"}

    # Load all records
    records = []
    links = []
    with Store(flux_path) as s, s.read() as txn:
        for r in s.scan(txn):
            records.append(r)
            for target_hex, rel in r.links:
                links.append((r.id, target_hex, rel))

    if not records:
        print("No records found in", flux_path)
        sys.exit(1)

    # Build id -> index map
    id_to_idx = {r.id: i for i, r in enumerate(records)}

    fig = plt.figure(figsize=(16, 6))

    # ── Panel 1: Record graph ──
    ax1 = fig.add_subplot(131)
    n = len(records)
    # Circular layout
    angles = np.linspace(0, 2 * np.pi, n, endpoint=False) if n > 1 else [0]
    xs = np.cos(angles)
    ys = np.sin(angles)

    for i, r in enumerate(records):
        layer_name = LAYER_NAMES.get(r.layer, "OTHER")
        color = LAYER_COLORS.get(r.layer, "#888")
        label = r.kind or r.id[:8]
        ax1.scatter(xs[i], ys[i], c=color, s=120, zorder=5, edgecolors="white", linewidth=0.5)
        offset = 0.15
        ax1.annotate(label, (xs[i], ys[i]), fontsize=6,
                     xytext=(xs[i] * (1 + offset), ys[i] * (1 + offset)),
                     ha="center", va="center")

    # Draw edges
    for src_hex, tgt_hex, rel in links:
        si = id_to_idx.get(src_hex)
        ti = id_to_idx.get(tgt_hex)
        if si is not None and ti is not None:
            ax1.annotate("", xy=(xs[ti], ys[ti]), xytext=(xs[si], ys[si]),
                         arrowprops=dict(arrowstyle="->", color="#AAA", lw=0.8))

    ax1.set_xlim(-1.5, 1.5)
    ax1.set_ylim(-1.5, 1.5)
    ax1.set_aspect("equal")
    ax1.set_title(f"Record Graph ({n} records, {len(links)} links)", fontsize=10)
    ax1.axis("off")

    # Legend
    for layer, name in LAYER_NAMES.items():
        count = sum(1 for r in records if r.layer & layer)
        if count:
            ax1.scatter([], [], c=LAYER_COLORS[layer], label=f"{name} ({count})", s=80)
    ax1.legend(fontsize=7, loc="lower center")

    # ── Panel 2: Layer breakdown ──
    ax2 = fig.add_subplot(132)
    layer_counts = {name: sum(1 for r in records if r.layer & mask)
                    for mask, name in LAYER_NAMES.items()}
    bars = ax2.barh(list(layer_counts.keys()), list(layer_counts.values()),
                    color=[LAYER_COLORS[m] for m in LAYER_NAMES])
    ax2.set_xlabel("Records")
    ax2.set_title("Layer Breakdown", fontsize=10)
    for bar, val in zip(bars, layer_counts.values()):
        if val:
            ax2.text(bar.get_width() + 0.1, bar.get_y() + bar.get_height()/2,
                     str(val), va="center", fontsize=9)

    # ── Panel 3: Kind distribution ──
    ax3 = fig.add_subplot(133)
    kind_counts = {}
    for r in records:
        k = r.kind or "(none)"
        kind_counts[k] = kind_counts.get(k, 0) + 1
    sorted_kinds = sorted(kind_counts.items(), key=lambda x: -x[1])[:10]
    if sorted_kinds:
        kinds, counts = zip(*sorted_kinds)
        ax3.barh(range(len(kinds)), counts, color="#5B9BD5")
        ax3.set_yticks(range(len(kinds)))
        ax3.set_yticklabels(kinds, fontsize=7)
        ax3.set_xlabel("Records")
        ax3.set_title("Kind Distribution (top 10)", fontsize=10)
        ax3.invert_yaxis()

    plt.suptitle(f"FLUXmeme: {os.path.basename(flux_path)}", fontsize=13, fontweight="bold")
    plt.tight_layout()

    if save_path:
        plt.savefig(save_path, dpi=150, bbox_inches="tight")
        print(f"Saved: {save_path}")
    else:
        plt.show()

    # ── Stats summary ──
    total = len(records)
    total_links = len(links)
    total_bytes = os.path.getsize(flux_path)
    print(f"\n{'='*50}")
    print(f"  File:     {flux_path}")
    print(f"  Size:     {total_bytes:,} bytes")
    print(f"  Records:  {total}")
    print(f"  Links:    {total_links}")
    for mask, name in LAYER_NAMES.items():
        c = layer_counts[name]
        if c:
            print(f"  {name:8s}:  {c} records")
    print(f"{'='*50}")


if __name__ == "__main__":
    main()
