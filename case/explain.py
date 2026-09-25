"""Illustrated section through one upper retainer. mm; display omitted from vendor mesh."""
from pathlib import Path
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle, Polygon
ROOT=Path(__file__).resolve().parent
fig,ax=plt.subplots(figsize=(12,7),facecolor='#faf8f2')
ax.set_facecolor('#faf8f2')
def rect(x,y,w,h,color,z=2):
    ax.add_patch(Rectangle((x,y),w,h,facecolor=color,edgecolor='#24373a',lw=1,zorder=z))
# Horizontal axis follows the device height; vertical axis is distance off the fridge.
rect(93,0,18,3.4,'#38868a')
rect(103.35,3.4,7.25,8.75,'#38868a')
rect(93,3.4,10,8.5,'#dedbd2')
# Pilot and screw relief, insert body, tab and screw.
rect(105.3,7.95,3,4.2,'#faf8f2',3)
rect(105.6,6.5,2.4,1.45,'#faf8f2',3)
rect(105.2,8.15,3.2,4,'#d7aa4c',4)
rect(102.3,12.15,8.3,2.8,'#82b9b5',4)
rect(105.6,12.15,2.4,2.8,'#faf8f2',5)
rect(105.8,6.95,2,8,'#536068',6)
rect(104.9,14.95,3.8,2,'#536068',6)
def note(text,xy,pos):
    ax.annotate(text,xy=xy,xytext=pos,fontsize=11,color='#24373a',ha='left',va='center',
                arrowprops=dict(arrowstyle='->',color='#24373a',lw=1.3),zorder=10,
                bbox=dict(facecolor='#faf8f2',edgecolor='none',pad=3))
note('Removable retaining tab', (109,13.5),(111.9,16.5))
note('M2 × 8 screw\nfastens tab to mount', (106.8,16),(111.9,20.5))
note('Your brass M2 insert\nØ3.2 × 4 mm; heat-set flush', (108.2,10),(111.9,11))
note('Solid post supports the tab\nTightening does not squeeze the board', (110,5),(111.9,4.4))
note('0.7 mm overlap catches the CASE rim\n0.25 mm gap above it', (102.65,12),(91,19))
note('Board in its factory case', (98,8),(91,9))
note('Printed frame / fridge behind this face', (99,0),(92,-2.5))
ax.text(91,24,'What the two small plates do',fontsize=22,weight='bold',color='#24373a')
ax.text(91,22.5,'SIDE SECTION THROUGH ONE UPPER TAB  •  same arrangement at the other corner',fontsize=10,color='#496468')
ax.text(91,-5,'The lower hooks carry the board. These upper tabs stop it tipping out.\nUndo the two screws to remove the board; nothing needs to bend.',fontsize=12,color='#24373a')
ax.set_xlim(90,129);ax.set_ylim(-7,26);ax.set_aspect('equal');ax.axis('off')
fig.savefig(ROOT/'retainer-explained.png',dpi=170,bbox_inches='tight',facecolor=fig.get_facecolor())
