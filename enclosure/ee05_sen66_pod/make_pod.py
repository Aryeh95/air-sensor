"""
Generates a SEN66 side-pod for the XIAO ePaper EE05 + 4.26" (GDEY0426T82) enclosure
(the "Cattt Casing" / EE05 frame STLs), plus an optional drilled copy of that back plate.

The pod bolts onto the right-hand side of the back plate using the two existing
right-hand screws, which pass through tabs on the pod. The SEN66 sits on its side
inside the pod with its inlets/outlet facing sideways through two windows; the
sensor's cable channel lines up with a single hole in the back plate.

All coordinates are in the original enclosure's assembly frame (mm):
  +X = right (seen from the front), +Y = down, -Z = towards the back.

Usage:
  pip install trimesh manifold3d numpy
  python make_pod.py                              # pod only
  python make_pod.py --backplate "Backplate.stl"  # pod + back plate with cable hole
  python make_pod.py --assembly                   # also export the pod in assembly position
"""

import argparse
import os

import numpy as np
import trimesh
from manifold3d import CrossSection, JoinType, Manifold, Mesh

# --- Existing enclosure geometry (measured from the STLs) ---
PLATE_BACK_Z = -48.16      # rear face of the back plate
PLATE_FRONT_Z = -46.66     # front face of the back plate
FRAME_BACK_Z = -48.30      # the frame's rim sits slightly proud of the back plate
FRAME_RIGHT_X = 55.89      # outer right face of the frame
SCREW_TOP = (37.36, -36.35)
SCREW_BOTTOM = (37.47, 36.18)

# --- SEN66 package (datasheet: 55.2 x 25.6 x 21.3 mm) ---
SEN_LENGTH = 55.2          # along Y
SEN_WIDTH = 25.6           # along Z (depth off the back plate)
SEN_HEIGHT = 21.3          # along X (connector face -> inlet/outlet face)

# --- Pod parameters ---
WALL = 1.6
CLEARANCE = 0.3
TAB_THICKNESS = 2.0
TAB_RADIUS = 3.5
SCREW_CLEARANCE_D = 3.2    # clears M2 / M2.5 / M3
CAVITY_TOP_Y = -30.0       # keeps clear of the top screw head
CORNER_RADIUS = 2.0
SEGMENTS = 64

# Derived cavity
CAV_X1 = FRAME_RIGHT_X - WALL
CAV_X0 = CAV_X1 - (SEN_HEIGHT + 2 * CLEARANCE)
CAV_Y0 = CAVITY_TOP_Y
CAV_Y1 = CAV_Y0 + SEN_LENGTH + 2 * CLEARANCE
CAV_Z1 = FRAME_BACK_Z - 0.05  # pod mating face, just clear of the frame rim
CAV_Z0 = CAV_Z1 - (SEN_WIDTH + 2 * CLEARANCE)

# Sensor placement: inlets at the bottom, outlet (fan) at the top so warm exhaust rises
# away from the inlets. u runs along the sensor length from the inlet end, v across its width.
SEN_Y_INLET_END = CAV_Y1 - CLEARANCE
SEN_Z_BACK = CAV_Z0 + CLEARANCE


def sensor_y(u):
    return SEN_Y_INLET_END - u


def sensor_z(v):
    return SEN_Z_BACK + v


def box(x0, x1, y0, y1, z0, z1):
    return Manifold.cube([x1 - x0, y1 - y0, z1 - z0]).translate([x0, y0, z0])


def rounded_rect(x0, x1, y0, y1, r):
    return CrossSection.square([x1 - x0 - 2 * r, y1 - y0 - 2 * r]).translate([x0 + r, y0 + r]).offset(
        r, JoinType.Round, circular_segments=SEGMENTS)


def xy_prism(section, z0, z1):
    return section.extrude(z1 - z0).translate([0, 0, z0])


def x_window_rect(y0, y1, z0, z1, r):
    """Rounded rectangular hole through the outer (+X) wall."""
    # Build in the XY plane (x->y, y->z) then rotate so the extrusion runs along +X.
    sec = CrossSection.square([y1 - y0 - 2 * r, z1 - z0 - 2 * r]).translate([y0 + r, z0 + r]).offset(
        r, JoinType.Round, circular_segments=SEGMENTS)
    solid = sec.extrude(10.0)  # along local Z
    # local (a, b, c) = (y, z, x): map with a transform matrix
    m = np.array([[0, 0, 1, CAV_X1 - 1.0],
                  [1, 0, 0, 0],
                  [0, 1, 0, 0]], dtype=float)
    return solid.transform(m)


def x_window_circle(yc, zc, d):
    cyl = Manifold.cylinder(10.0, d / 2, circular_segments=SEGMENTS)
    m = np.array([[0, 0, 1, CAV_X1 - 1.0],
                  [1, 0, 0, yc],
                  [0, 1, 0, zc]], dtype=float)
    return cyl.transform(m)


def build_pod():
    outer = xy_prism(rounded_rect(CAV_X0 - WALL, FRAME_RIGHT_X, CAV_Y0 - WALL, CAV_Y1 + WALL, CORNER_RADIUS),
                     CAV_Z0 - WALL, CAV_Z1)
    cavity = box(CAV_X0, CAV_X1, CAV_Y0, CAV_Y1, CAV_Z0, CAV_Z1 + 1.0)
    pod = outer - cavity

    # Screw tabs sitting flat on the back plate
    tab_x = (SCREW_TOP[0] + SCREW_BOTTOM[0]) / 2
    for sx, sy, pod_edge in ((SCREW_TOP[0], SCREW_TOP[1], CAV_Y0 - WALL / 2),
                             (SCREW_BOTTOM[0], SCREW_BOTTOM[1], CAV_Y1 + WALL / 2)):
        pad = CrossSection.circle(TAB_RADIUS, SEGMENTS).translate([sx, sy])
        root = CrossSection.square([2 * TAB_RADIUS, 0.01]).translate([tab_x - TAB_RADIUS, pod_edge])
        tab = xy_prism(CrossSection.batch_hull([pad, root]), CAV_Z1 - TAB_THICKNESS, CAV_Z1)
        hole = Manifold.cylinder(20, SCREW_CLEARANCE_D / 2, circular_segments=SEGMENTS).translate(
            [sx, sy, CAV_Z1 - 10])
        pod = (pod + tab) - hole

    # Inlet window: covers the square + round inlet (datasheet: u 3.2-10.2, v 2.8-21.2),
    # sized symmetrically in v so the sensor works either way round.
    pod -= x_window_rect(sensor_y(11.2), sensor_y(2.2), sensor_z(1.8), sensor_z(23.8), 1.5)
    # Outlet window: fan outlet is a 20.5 mm circle centred at u=42.4, v=12.8
    pod -= x_window_circle(sensor_y(42.4), sensor_z(12.8), 21.0)
    return pod


def cable_hole():
    """Hole through the back plate where the SEN66 cable channel (u 19-29.9) meets the plate."""
    y0, y1 = sensor_y(29.9) - 0.5, sensor_y(19.0) + 0.5
    x0, x1 = CAV_X0 + 0.8, CAV_X0 + 9.0
    return xy_prism(rounded_rect(x0, x1, y0, y1, 2.0), PLATE_BACK_Z - 1.0, PLATE_FRONT_Z + 1.0)


def sensor_dummy():
    return box(CAV_X0 + CLEARANCE, CAV_X1 - CLEARANCE, CAV_Y0 + CLEARANCE, CAV_Y1 - CLEARANCE,
               CAV_Z0 + CLEARANCE, CAV_Z1 - CLEARANCE)


def to_trimesh(m):
    mesh = m.to_mesh()
    return trimesh.Trimesh(vertices=np.asarray(mesh.vert_properties)[:, :3], faces=np.asarray(mesh.tri_verts))


def from_trimesh(t):
    return Manifold(Mesh(vert_properties=np.asarray(t.vertices, dtype=np.float32),
                         tri_verts=np.asarray(t.faces, dtype=np.uint32)))


def print_orientation(m):
    """Plate side down: no supports needed (the back wall bridges 22 mm)."""
    t = to_trimesh(m)
    t.apply_transform(trimesh.transformations.rotation_matrix(np.pi, [0, 1, 0]))
    t.apply_translation(-t.bounds[0])
    return t


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--backplate", help="original back plate STL; writes a copy with the cable hole")
    parser.add_argument("--assembly", action="store_true", help="also write the pod in assembly position")
    parser.add_argument("--out", default=os.path.dirname(os.path.abspath(__file__)))
    args = parser.parse_args()

    pod = build_pod()
    print_orientation(pod).export(os.path.join(args.out, "sen66_pod.stl"))
    if args.assembly:
        to_trimesh(pod).export(os.path.join(args.out, "sen66_pod_assembly_position.stl"))
    print(f"pod: {pod.volume():.0f} mm^3, bounds {np.round(pod.bounding_box(), 2)}")

    if args.backplate:
        plate = from_trimesh(trimesh.load(args.backplate)) - cable_hole()
        name = os.path.splitext(os.path.basename(args.backplate))[0].strip() + "_sen66_cable_hole.stl"
        to_trimesh(plate).export(os.path.join(args.out, name))
        print(f"wrote {name}")


if __name__ == "__main__":
    main()
