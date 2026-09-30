# SEN66 side-pod for the EE05 + 4.26" enclosure

An add-on housing that mounts a Sensirion SEN66 on the back of the EE05 / 4.26" e-paper
enclosure (the "Cattt Casing": EN05 frame, support, back plate, back cap, stand, switch cap).
None of the original parts need reprinting. You only need one cable hole in the back plate.

- **`sen66_pod.stl`**: ready to print, already in print orientation.
- **`make_pod.py`**: generates the pod (and optionally a copy of the back plate with the cable hole).

![The case with the pod attached: back, side and front](render.png)

## How it fits

- The pod sits on the back plate beside the EE05 pocket, on the **right when you look at the back**
  of the case (the left when you look at the screen). It is held on by the **two existing
  back-plate screws on that side**, which pass through tabs on the pod.
- The SEN66 lies on its side with its inlets and outlet facing sideways through two windows in the
  pod's outer wall: inlets at the bottom, fan outlet at the top. Sensirion's guidelines allow
  sideways-facing openings, the solid wall between the windows keeps inlet and outlet air apart, and
  most of the sensor sits below the EE05 (the ESP32 is the main heat source).
- The sensor cable leaves through the channel under the sensor and goes into the case through one
  hole in the back plate.
- Checked against the original STLs: no overlap with any part, and about 2 mm of clearance from
  the desk when the case leans back on its stand (about 20°).

## Hardware

The original design doesn't list any. These sizes come from measuring its STLs and the EE05's PCB file:

| Qty | Screw | Goes |
| --- | --- | --- |
| 2 | **M3 × 8 mm**, pan or button head | Back plate to frame: the two screws on the left, seen from the back |
| 2 | **M3 × 10 mm**, pan or button head | Through the pod's tabs and the back plate into the frame (the pod side) |
| 4 | **M2 × 6 mm** (M2 × 8 also fits) | EE05 board onto the four posts on the display support |

- The frame's four bosses have 2.5 mm holes, 8 mm deep: the standard drill size for tapping an M3
  thread. The screws cut their own thread in the plastic. Screws made for plastic ("PT" or
  thread-forming) are ideal, but ordinary M3 machine screws work in PLA or PETG too. Stop once
  they're snug.
- The EE05's mounting holes are 2.5 mm (M2 clearance) and the display support's posts have 2.0 mm
  pilot holes, 10 mm deep.
- The original back plate's holes are also 2.5 mm, too tight for M3 to pass through. The back
  plate generated here has them opened to 3.2 mm. If you print the original instead, drill its four
  holes out to 3.2 mm (a 1/8" bit is fine).
- Maximum lengths before a screw bottoms out: M3 × 10 for the plain back-plate screws, M3 × 12 for
  the pod screws.
- The back cap, stand and switch cap have no screw holes.

## Printing

- Print `sen66_pod.stl` as is (tabs flat on the bed). No supports: the back wall bridges 22 mm.
- 0.2 mm layers, 3 walls, PLA or PETG.

## Assembly

1. **Cable hole.** Either
   - drill a ~9 mm hole in the back plate. Looking at the outside of the back plate (from behind
     the case), it's centred on the line between the two right-hand screw holes, ~1 mm below
     halfway between them: 17.5 mm in from the right-hand edge and 43 mm down from the top edge.
     Or
   - generate a back plate with the hole (and 3.2 mm screw holes) already in it:
     `python make_pod.py --backplate "Backplate.stl"`
2. Feed the bare-wire end of the SEN66 cable through the hole from the outside and plug the
   connector into the sensor. Route the cable through the channel under the sensor.
3. Put the SEN66 into the pod with its inlet/outlet face against the windows (round fan outlet in
   the top window) and the connector side against the inner wall.
4. Put the pod, with the sensor in it, onto the back plate and fix it with the two screws on that
   side (the right, seen from the back): **M3 × 10**, 2 mm longer than the others because of the
   2 mm tabs.
5. Seal the hole around the cable from the inside with a bit of Blu-Tack, so the SEN66 can't draw
   air from inside the case.
6. Optional: a strip of thin foam tape on the sensor face between the inlets and the outlet
   improves the seal against the pod wall.

## Regenerating / tweaking

```
pip install trimesh manifold3d numpy
python make_pod.py [--backplate Backplate.stl] [--assembly]
```

All dimensions (wall thickness, clearance, tab size, screw hole size, pod position) are
constants at the top of `make_pod.py`. `--assembly` also writes the pod in the enclosure's
assembly coordinates, so you can load it together with the original STLs to check the fit.

This pod is designed from measurements of the enclosure STLs and the SEN66 datasheet. It hasn't
been test-printed yet, so print it and dry-fit it before final assembly.
