"""PaperColor C151 fridge cradle, mm. Run with CadQuery 2.8 + trimesh.
Optional first argument: official PaperColor.stl (exploded multi-copy reference).
"""
from pathlib import Path
import sys, json
import cadquery as cq
import trimesh
ROOT = Path(__file__).resolve().parent
W,H,T = 70.,103.,8.5
CLEARANCE, FLOOR, WALL = .35,3.4,2.
MAG_D,MAG_DEPTH = 6.3,2.1  # 6 x 2 mm discs; glue, not force-fit.
SEAT = FLOOR
FRONT = SEAT+T

def box(x,y,z,w,h,d):
    return cq.Workplane('XY').box(w,h,d,centered=False).translate((x,y,z))
def rounded(x,y,z,w,h,d,r):
    return box(x,y,z,w,h,d).edges('|Z').fillet(r)
def cyl(x,y,z,d,h):
    return cq.Workplane('XY').circle(d/2).extrude(h).translate((x,y,z))

# Open centre reduces material and exposes the rear; no modification to device.
body = rounded(-2.35,-2.35,0,74.7,107.7,FLOOR,4)
body = body.cut(rounded(10,11,-.1,50,81,FLOOR+.2,4))
mag_centres = [(x,y) for x in (3.5,66.5) for y in (3.5,99.5)]
for x,y in mag_centres:
    body = body.cut(cyl(x,y,-.1,MAG_D,MAG_DEPTH+.1))
# Short side stops only at corners: all side controls and ports remain exposed.
for x in (-2.35,W+CLEARANCE):
    for y in (1,98):
        body = body.union(box(x,y,FLOOR-.1,WALL,4,6.1))
# V3: no flex-to-fit clips. Broad lower hooks and removable upper retainers.
# Lower root flares through a 3 mm radius into a full-height foot.
for x in (20,50):
    body=body.union(rounded(x-2,-6.55,0,14,12,FLOOR,1.5))
    lower=(cq.Workplane('YZ').moveTo(-.35,0).lineTo(-6.55,0)
           .lineTo(-6.55,FLOOR)
           .threePointArc((-4.42868,FLOOR+.87868),(-3.55,FLOOR+3))
           .lineTo(-3.55,FRONT+2.9).lineTo(-.35,FRONT+2.9)
           .lineTo(.7,FRONT+1.85).lineTo(.7,FRONT+.25)
           .lineTo(-.35,FRONT+.25).close().extrude(10).translate((x,0,0)))
    body=body.union(lower)
# Hardware: M2 x 8 screws and OD3.2 x 4 heat-set brass inserts.
# Pilot 3.0 mm, 4.2 mm deep with screw-tip relief below.
POST_TOP=FRONT+.25
CAP_T=2.8
INSERT_OD,INSERT_LEN,INSERT_PILOT=3.2,4.,3.
SCREWS=[(13,H+3.8),(59,H+3.8)]
retainers=[]
for x,y in SCREWS:
    body=body.union(rounded(x-8,H-6,0,16,14,FLOOR,2))
    post=rounded(x-6,H+.35,0,12,7.25,POST_TOP,1)
    body=body.union(post)
    # Install inserts from the front, flush with each post, before adding the board.
    body=body.cut(cyl(x,y,POST_TOP-4.2,INSERT_PILOT,4.3))
    body=body.cut(cyl(x,y,6.5,2.4,POST_TOP-6.5+.1))
    cap=rounded(x-6,H-.7,POST_TOP,12,8.3,CAP_T,.8)
    cap=cap.cut(cyl(x,y,POST_TOP-.1,2.4,CAP_T+.2))
    # Two short locating pins prevent a one-screw retainer from swivelling.
    for dx in (-3.8,3.8):
        body=body.union(cyl(x+dx,y,POST_TOP-.1,2.,1.1))
        cap=cap.cut(cyl(x+dx,y,POST_TOP-.1,2.5,1.3))
    retainers.append(cap)
# Recut pockets after adding feet so the original magnet fit stays exact.
for x,y in mag_centres:
    body=body.cut(cyl(x,y,-.1,MAG_D,MAG_DEPTH+.1))
body=body.clean()
# Print both retainers with their flat outer/head face on the bed, pin recesses up.
cap_print=retainers[0].rotate((0,0,0),(1,0,0),180)
bb=cap_print.val().BoundingBox()
cap_print=cap_print.translate((-bb.xmin,-bb.ymin,-bb.zmin))
assembly=cq.Compound.makeCompound([body.val()]+[c.val() for c in retainers])
# A small pocket coupon tests the same printer orientation and pocket depth.
coupon=rounded(0,0,0,30,12,FLOOR,2)
for x,d in [(6,6.2),(15,6.3),(24,6.4)]:
    coupon=coupon.cut(cyl(x,6,-.1,d,MAG_DEPTH+.1))

results={}
for name,obj in [('papercolor-fridge-mount',body),('magnet-fit-6mm',coupon),('upper-retainer-PRINT-2',cap_print)]:
    assert obj.val().isValid() and len(obj.solids().vals())==1
    cq.exporters.export(obj,str(ROOT/(name+'.stl')),tolerance=.035,angularTolerance=.08)
    mesh=trimesh.load(ROOT/(name+'.stl'))
    assert mesh.is_watertight and mesh.is_winding_consistent and mesh.volume>0
    assert len(mesh.split())==1
    results[name]={'watertight':True,'single_solid':True,'dimensions_mm':mesh.extents.tolist(),'volume_cm3':mesh.volume/1000}
cq.exporters.export(assembly,str(ROOT/'papercolor-fridge-mount.step'))
# One STL containing three disconnected printable parts, all on Z=0.
plate=cq.Compound.makeCompound([body.val(),cap_print.translate((80,20,0)).val(),cap_print.translate((80,38,0)).val()])
cq.exporters.export(plate,str(ROOT/'papercolor-fridge-mount-v3-m2-plate.stl'),tolerance=.035,angularTolerance=.08)
plate_mesh=trimesh.load(ROOT/'papercolor-fridge-mount-v3-m2-plate.stl')
assert plate_mesh.is_watertight and len(plate_mesh.split())==3
assert all(abs(part.bounds[0,2])<1e-5 for part in plate_mesh.split())
# Regression: root section at reported failure height, hook tips and seating gap.
for x in (20,50):
    assert body.val().isInside(cq.Vector(x+5,-4,FLOOR+1))
    assert body.val().isInside(cq.Vector(x+5,.3,FRONT+1.5))
    assert not body.val().isInside(cq.Vector(x+5,.3,FRONT+.1))
for (x,y),cap in zip(SCREWS,retainers):
    assert body.val().intersect(cap.val()).Volume()<1e-6
    # Full screw shank plus head envelope clears plastic, except intended bearing face.
    screw=cyl(x,y,POST_TOP+CAP_T-8,2.,8).union(cyl(x,y,POST_TOP+CAP_T,3.8,2.))
    assert body.val().intersect(screw.val()).Volume()<1e-6
    assert cap.val().intersect(screw.val()).Volume()<1e-6
    # Full 4 mm insert engagement; screw clears the blind-hole floor.
    assert POST_TOP+CAP_T-8 < POST_TOP-INSERT_LEN
    assert POST_TOP+CAP_T-8 > 6.5
    assert not body.val().isInside(cq.Vector(x+1.4,y,POST_TOP-4.1))
    assert body.val().isInside(cq.Vector(x+1.4,y,POST_TOP-4.4))
    # Cap lips lie above the enclosure and off the active display.
    assert cap.val().isInside(cq.Vector(x,H-.4,POST_TOP+.5))
results['revision']='v3 rigid PLA / M2x8 screws and OD3.2 x 4 heat-set inserts'
results['print_plate']={'watertight':True,'parts':3,'dimensions_mm':plate_mesh.extents.tolist()}
results['root_hook_and_fastener_checks']='passed'

# Analytic section checks: four blind magnet pockets with >=1.3 mm roof.
for x,y in mag_centres:
    assert not body.val().isInside(cq.Vector(x,y,1))
    assert body.val().isInside(cq.Vector(x,y,2.2))
assert FLOOR-MAG_DEPTH>=1.29
results['physical_validation']='V1 reported size okay, all four clips fractured about 1 mm above frame in Bambu PLA Matte. V3 unprinted; strength, assembly and fridge retention still need physical checks.'

# VTK provides both reference collision checks and depth-correct mesh previews.
import vtk
from vtk.util.numpy_support import numpy_to_vtk

def poly(mesh):
    p=vtk.vtkPolyData(); pts=vtk.vtkPoints()
    pts.SetData(numpy_to_vtk(mesh.vertices,deep=True));p.SetPoints(pts)
    cells=vtk.vtkCellArray()
    for f in mesh.faces:
        cells.InsertNextCell(3)
        for i in f: cells.InsertCellPoint(int(i))
    p.SetPolys(cells);return p
mount=trimesh.load(ROOT/'papercolor-fridge-mount.stl')
# Mesh assembled retainers for collision checking and the front view only.
verts,faces=assembly.tessellate(.035)
assembled=trimesh.Trimesh(vertices=[v.toTuple() for v in verts],faces=faces,process=True)
reference=None
if len(sys.argv)>1:
    raw=trimesh.load(sys.argv[1])
    reference=trimesh.util.concatenate([p for p in raw.split(only_watertight=False) if p.bounds[0,0]>-2])
    reference.apply_translation((-.1498566,-.011651339,T+SEAT))
    # Lift excludes intentional coplanar rear seating contact.
    lifted=reference.copy();lifted.apply_translation((0,0,.03))
    check=vtk.vtkCollisionDetectionFilter()
    for i,m in enumerate((assembled,lifted)):
        check.SetInputData(i,poly(m));check.SetTransform(i,vtk.vtkTransform())
    check.SetCollisionModeToAllContacts();check.Update()
    results['vendor_surface_intersections_excluding_seat']=check.GetNumberOfContacts()
    assert check.GetNumberOfContacts()==0, results
win=vtk.vtkRenderWindow();win.SetOffScreenRendering(1);win.SetSize(1600,1000);win.SetMultiSamples(8)
for i in range(2):
    ren=vtk.vtkRenderer();ren.SetViewport(i*.5,0,(i+1)*.5,1);ren.SetBackground(.965,.953,.929);win.AddRenderer(ren)
    items=[(assembled if i==0 else mount,(.20,.49,.52))]
    if i==0 and reference is not None: items.append((reference,(.80,.79,.75)))
    for mesh,color in items:
        mapper=vtk.vtkPolyDataMapper();mapper.SetInputData(poly(mesh))
        actor=vtk.vtkActor();actor.SetMapper(mapper);actor.GetProperty().SetColor(*color)
        actor.GetProperty().SetAmbient(.3);actor.GetProperty().SetDiffuse(.7);ren.AddActor(actor)
    camera=ren.GetActiveCamera();camera.ParallelProjectionOn();camera.SetViewUp(0,1,0)
    camera.SetFocalPoint(35,51,5);camera.SetPosition(125,-60,240 if i==0 else -230);camera.SetParallelScale(83)
    label=vtk.vtkTextActor();label.SetInput('V3 / M2 heat-set retainers' if i==0 else 'BACK / four 6 x 2 mm pockets')
    label.GetTextProperty().SetFontSize(23);label.GetTextProperty().SetColor(.12,.17,.18);label.SetPosition(40,920);ren.AddViewProp(label)
    ren.ResetCameraClippingRange()
win.Render()
capture=vtk.vtkWindowToImageFilter();capture.SetInput(win);capture.ReadFrontBufferOff();capture.Update()
writer=vtk.vtkPNGWriter();writer.SetFileName(str(ROOT/'preview.png'));writer.SetInputConnection(capture.GetOutputPort());writer.Write();win.Finalize()
(ROOT/'validation.json').write_text(json.dumps(results,indent=2)+'\n')
print(json.dumps(results,indent=2))
