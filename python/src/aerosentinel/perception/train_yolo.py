"""D1 training entry point.  python -m aerosentinel.perception.train_yolo --data data/hazards.yaml

Dataset prep: images + labels exported from Unreal (see unreal/AeroSentineal/Simulation) in YOLO format;
write a data YAML with classes: flood, fire, smoke, landslide, crack.
"""
from __future__ import annotations

import argparse


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--data", required=True)
    ap.add_argument("--base", default="yolov8n.pt")
    ap.add_argument("--epochs", type=int, default=50)
    ap.add_argument("--imgsz", type=int, default=640)
    a = ap.parse_args()
    from ultralytics import YOLO
    YOLO(a.base).train(data=a.data, epochs=a.epochs, imgsz=a.imgsz, project="runs/perception")


if __name__ == "__main__":
    main()
