# SEN66 side-pod for the EE05 + 4.26" enclosure

An add-on housing that mounts a Sensirion SEN66 on the back of the EE05 / 4.26" e-paper
enclosure (the "Cattt Casing": EN05 frame, support, back plate, back cap, stand, switch cap).
None of the original parts need reprinting. You only need one cable hole in the back plate.

- **`sen66_pod.stl`**: ready to print, already in print orientation.
- **`make_pod.py`**: generates the pod (and optionally a copy of the back plate with the cable hole).

## How it fits

- The pod sits on the right-hand side of the back plate (right as seen from the front, so on your
  left when you look at the back), beside the EE05 pocket. It is held on by the **two existing
  right-hand back-plate screws**, which pass through tabs on the pod.
- The SEN66 lies on its side with its inlets and outlet facing sideways through two windows in the
  pod's outer wall: inlets at the bottom, fan outlet at the top. Sensirion's guidelines allow
  sideways-facing openings, the solid wall between the windows keeps inlet and outlet air apart, and
  most of the sensor sits below the EE05 (the ESP32 is the main heat source).
- The sensor cable leaves through the channel under the sensor and goes into the case through one
  hole in the back plate.
- Checked against the original STLs: no overlap with any part, and about 2 mm of clearance from
  the desk when the case leans back on its stand (about 20°).

## Printing

- Print `sen66_pod.stl` as is (tabs flat on the bed). No supports: the back wall bridges 22 mm.
- 0.2 mm layers, 3 walls, PLA or PETG.

## Assembly

1. **Cable hole.** Either
   - drill a ~9 mm hole in the back plate, centred on the line between the two right-hand screw
     holes and ~1 mm below halfway between them (17.5 mm in from the right-hand edge of the back
     plate, 43 mm down from its top edge), or
   - generate a back plate with the hole already in it:
     `python make_pod.py --backplate "Backplate.stl"`
2. Feed the bare-wire end of the SEN66 cable through the hole from the outside and plug the
   connector into the sensor. Route the cable through the channel under the sensor.
3. Put the SEN66 into the pod with its inlet/outlet face against the windows (round fan outlet in
   the top window) and the connector side against the inner wall.
4. Put the pod, with the sensor in it, onto the back plate and fix it with the two right-hand
   screws. They need to be **2 mm longer** than the originals because of the 2 mm tabs.
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
