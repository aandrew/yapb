"""Map the supplied Dust2 callout diagram to YaPB graph nodes.

Run from the repository root. No game binary is changed. Pillow is optional and
only needed for --render. All fine-area boundaries remain explicit drafts.
"""
import argparse, hashlib, json, struct
from pathlib import Path

SOURCE='https://www.reddit.com/media?url=https%3A%2F%2Fpreview.redd.it%2Fcan-you-help-me-finish-this-cs-1-6-callouts-v0-qfi9qopdj6xf1.png%3Fwidth%3D1024%26format%3Dpng%26auto%3Dwebp%26s%3D942fb5c5c546e0d92709b6777d96809bd71fe5fd'
# The reference is the native 1024x768 overview placed at y=150 on a 1024x1024
# canvas. Native GoldSrc overview: zoom=1.5, origin=(-223,1097), rotated=0.
def world_to_reference(x,y):return (717.6875-y*0.1875,492.1875-x*0.1875)
def reference_to_world(x,y):return ((492.1875-y)/0.1875,(717.6875-x)/0.1875)

def read_graph(path):
    data=Path(path).read_bytes()
    magic,version,options,count,compressed,expected=struct.unpack_from('<6i',data)
    if magic not in (1497452610,) or version!=2 or not 8<=count<=4096 or expected!=count*220:
        raise ValueError('Unsupported YaPB graph layout')
    src=data[24:24+compressed];out=bytearray();i=0
    def varint():
        nonlocal i
        value=0
        for shift in range(0,29,7):
            if i>=len(src):raise ValueError('Truncated ULZ varint')
            b=src[i];i+=1;value+=b<<shift
            if b<128:return value
        return value
    while i<len(src):
        token=src[i];i+=1;run=token>>5
        if run:
            if run==7:run+=varint()
            if i+run>len(src):raise ValueError('Truncated literals')
            out.extend(src[i:i+run]);i+=run
            if i==len(src):break
        length=(token&15)+4
        if length==19:length+=varint()
        if i+2>len(src):raise ValueError('Truncated match')
        distance=((token&16)<<12)+int.from_bytes(src[i:i+2],'little');i+=2
        if distance==0 or distance>len(out):raise ValueError('Invalid ULZ match')
        for _ in range(length):out.append(out[-distance])
        if len(out)>expected:raise ValueError('Oversized ULZ output')
    if i!=len(src) or len(out)!=expected:raise ValueError('Graph size mismatch')
    nodes=[]
    for n in range(count):
        offset=n*220;index,flags,x,y,z=struct.unpack_from('<ii3f',out,offset)
        if index!=n:raise ValueError('Unexpected node numbering')
        links=[struct.unpack_from('<h',out,offset+56+k*20+18)[0]for k in range(8)]
        nodes.append({'id':index,'origin':[round(x,2),round(y,2),round(z,2)],'flags':flags,'links':[v for v in links if 0<=v<count]})
    return nodes,hashlib.sha256(data).hexdigest()

def area(key,label,box,parent=None,z=(-4096,4096),aliases=()):
    x0,y0,x1,y1=box
    poly=[reference_to_world(x,y)for x,y in [(x0,y0),(x1,y0),(x1,y1),(x0,y1)]]
    return {'id':key,'label':label,'aliases':list(aliases),'parent':parent,'reference_box':list(box),
        'polygon':[[round(x,2),round(y,2)]for x,y in poly],'z_range':list(z),'status':'draft_boundary'}

def definitions():
    a=[]
    def add(*args,**kwargs):a.append(area(*args,**kwargs))
    add('a_site','A Site',(225.7,252.2,285.7,300.2),aliases=('A','Bombsite A'))
    add('a_goose','Goose',(140,260,177,308),'a_site')
    add('a_default','Default',(266,250,287,304),'a_site',aliases=('A default plant',))
    add('a_ramp','A Ramp',(140,198,286,251),z=(40,4096))
    add('cross','Cross',(287,192,335,265),aliases=('A Cross',))
    add('billboard','Billboard / Reklama',(286,155,382,192),aliases=('Reklama',))
    add('long','Long',(334,190,586,248),aliases=('A Long','Long A'))
    add('long_corner','Long Corner',(455,238,501,288),'long')
    add('long_platform','Long Platform / Plat',(524,152,655,192),'long',aliases=('Plat',))
    add('pit','Pit',(574,194,718,249),'long')
    add('side_pit','Side Pit',(560,249,688,312),'pit')
    add('long_box','Long Blue Box',(489,297,558,370),'long',aliases=('Blue','Long Box'))
    add('long_doors','Long Doors',(582,346,674,413),'long')
    add('outside_long','Outside Long',(674,352,889,549),aliases=('Long Outside',))
    add('a_elevator','Elevator',(262,304,290,360),'a_site')
    add('ct_ramp','CT Ramp',(290,270,331,397),z=(-160,160))
    add('fast_cat','Fast Cat / Boost',(249,360,298,404),'short_a',z=(30,4096),aliases=('Fast Cat','Boost'))
    add('ninja','Ninja',(192,389,216,435),'short_a',z=(30,4096))
    add('secret','Secret',(207,428,264,463),'ct_spawn',z=(-160,40))
    add('short_a','Short A',(210,393,411,442),z=(40,4096),aliases=('Short','A Short'))
    add('cat_boost','Cat Boost',(417,385,454,418),'short_a',z=(30,4096))
    add('short_stairs','Short Stairs',(397,406,436,453),'short_a',z=(20,4096))
    add('cat','Cat',(400,451,433,522),'catwalk',z=(20,4096))
    add('catwalk','Catwalk',(430,511,627,550),z=(20,4096),aliases=('Cat Walk',))
    add('ct_spawn','CT Spawn',(225,385,335,535),z=(-180,20),aliases=('CT Base',))
    add('mid_doors','Mid Doors',(398,531,434,595),z=(-200,100),aliases=('Double Doors','Mid Double Doors'))
    add('xbox','X Box / Mid Box',(431,529,467,561),'middle',aliases=('Xbox','Mid Box'))
    add('middle','Middle',(434,549,588,591),aliases=('Mid',))
    add('top_mid','Top Mid',(588,547,668,590),'middle')
    add('suicide','Suicide',(668,555,835,598),'top_mid')
    add('mid_cover','Mid Cover',(589,596,662,634),'middle')
    add('t_spawn','T Spawn',(834,548,914,674),z=(80,4096),aliases=('T Base',))
    add('ct_mid','CT Mid',(211,537,336,683),z=(-200,100))
    add('mushrooms','Mushrooms',(210,628,280,684),'ct_mid')
    add('rock','Rock',(193,677,222,739),'ct_mid')
    add('b_tetris','Tetris / B Box',(280,672,335,725),'ct_mid',aliases=('Tetris','B Box'))
    add('b_doors','B Doors',(289,721,342,751))
    add('b_window','Window',(196,733,254,756),'b_site',aliases=('B Window',))
    add('b_site','B Site',(177.7,744.2,249.7,816.2),aliases=('B','Bombsite B'))
    add('b_back_site','Back Site',(172,742,195,818),'b_site')
    add('b_double_stack','Double Stack / Cigarette',(215,789,288,824),'b_site',aliases=('Double Stack','Cigarette'))
    add('b_big_box','Big Box',(218,822,276,847),'b_site')
    add('b_plat','B Plat',(140,823,223,859),'b_site',aliases=('B Platform',))
    add('b_back_plat','Back Plat / Element',(104,854,212,888),'b_site',aliases=('Element','Back Plat'))
    add('b_front_plate','Front Plate',(226,848,289,894),'b_site',aliases=('Front Plat',))
    add('b_fake','Fake',(289,856,326,893),'b_site')
    add('b_dog','Dog',(329,856,390,895),'b_site')
    add('b_jump_box','Jump Box',(297,815,365,853),'b_site')
    add('b_dark','Dark / Closet',(377,770,451,852),'b_site',aliases=('Dark','Closet'))
    add('b_ninety','90',(342,738,377,779),'b_site',aliases=('Ninety',))
    add('lower_tunnels','Lower Tunnels',(429,590,479,719),z=(-200,80),aliases=('Lower',))
    add('tunnel_stairs','Tunnel Stairs',(478,685,529,751))
    add('upper_tunnels','Upper Tunnels',(452,777,526,899),z=(0,4096),aliases=('Upper','B Tunnels'))
    add('outside_tunnels','Outside Tunnels / Ocean',(586,756,768,879),aliases=('Ocean','Outside Tunnels'))
    add('t_platform','T Platform / Titanic',(768,740,912,813),aliases=('Titanic','T Platform'))
    add('t_ramp','T Ramp',(768,813,912,882))
    add('kebab','Kebab / T Wall',(800,882,910,914),aliases=('Kebab','T Wall'))
    # Bomb zones are confirmed from BSP model bounds, not label-text positions.
    for entry in a:
        if entry['id'] in ('a_site','b_site'):entry['status']='bsp_zone_verified'
    return a

def contains(entry,point):
    x,y=world_to_reference(*point[:2]);x0,y0,x1,y1=entry['reference_box'];lo,hi=entry['z_range']
    return x0<=x<=x1 and y0<=y<=y1 and lo<=point[2]<=hi

def classify(areas,point):
    matches=[a for a in areas if contains(a,point)]
    if not matches:return None
    return min(matches,key=lambda a:(a['reference_box'][2]-a['reference_box'][0])*(a['reference_box'][3]-a['reference_box'][1]))

def build(graph):
    nodes,digest=read_graph(graph);areas=definitions()
    for entry in areas:
        entry['waypoint_ids']=[n['id']for n in nodes if contains(entry,n['origin'])]
        x0,y0,x1,y1=entry['reference_box'];center=reference_to_world((x0+x1)/2,(y0+y1)/2)
        members=[n for n in nodes if n['id'] in entry['waypoint_ids']]
        nearest=min(members,key=lambda n:(n['origin'][0]-center[0])**2+(n['origin'][1]-center[1])**2)if members else None
        entry['representative_waypoint']=nearest['id']if nearest else None
        entry['representative_origin']=nearest['origin']if nearest else None
    assigned=[n for n in nodes if classify(areas,n['origin'])]
    result={'version':1,'map':'de_dust2','source':SOURCE,'status':'draft_requires_in_game_review',
      'alignment':{'overview_zoom':1.5,'overview_origin':[-223,1097],'overview_size':[1024,768],
        'reference_y_offset':150,'reference_size':[1024,1024],'world_to_reference':{'x':[0,-0.1875,717.6875],'y':[-0.1875,0,492.1875]},
        'evidence':'Reference map geometry matches installed overviews/de_dust2.bmp at pixel offset (0,150); A/B bounds checked against BSP bomb-target models.'},
      'graph_sha256':digest,'graph_nodes':len(nodes),'covered_nodes':len(assigned),
      'unmapped_waypoints':[n['id']for n in nodes if not classify(areas,n['origin'])],
      'areas':areas}
    return result,nodes

def render(result,nodes,overview,output):
    from PIL import Image,ImageDraw,ImageFont
    canvas=Image.new('RGB',(1540,835),'#171c24')
    base=Image.open(overview).convert('RGB')
    # Replace the engine's chroma-key background before drawing graph points.
    pixels=base.load()
    for y in range(base.height):
        for x in range(base.width):
            r,g,b=pixels[x,y]
            if g>220 and r<30 and b<30:pixels[x,y]=(23,28,36)
    canvas.paste(base,(0,55));draw=ImageDraw.Draw(canvas)
    draw.text((15,12),'DUST2 CALLOUTS -> YaPB WAYPOINTS | DRAFT AREA BOUNDARIES',fill='white',font=ImageFont.truetype('C:/Windows/Fonts/arialbd.ttf',20))
    colors=['#ffcc66','#65d6ad','#61b8ff','#e58fff','#ff8a85','#f5f178']
    for node in nodes:
        px,py=world_to_reference(*node['origin'][:2]);py=py-150+55
        entry=classify(result['areas'],node['origin'])
        color=colors[result['areas'].index(entry)%len(colors)]if entry else '#626a77'
        draw.ellipse((px-2,py-2,px+2,py+2),fill=color)
    font=ImageFont.truetype('C:/Windows/Fonts/arial.ttf',10)
    for i,entry in enumerate(result['areas']):
        x0,y0,x1,y1=entry['reference_box'];y0=y0-150+55;y1=y1-150+55
        draw.rectangle((x0,y0,x1,y1),outline=colors[i%len(colors)],width=1)
        draw.text(((x0+x1)/2,(y0+y1)/2),str(i+1),fill='white',font=font,stroke_width=1,stroke_fill='black')
    label=ImageFont.truetype('C:/Windows/Fonts/arial.ttf',13)
    for i,entry in enumerate(result['areas']):
        draw.text((1030,58+i*13),f'{i+1:02d} {entry["label"]} ({len(entry["waypoint_ids"])} nodes)',fill=colors[i%len(colors)],font=label)
    draw.text((15,815),f'{result["graph_nodes"]} nodes | {result["covered_nodes"]} inside draft areas | Grey = unmapped. Fine labels require review; A/B zones verified from BSP.',fill='white',font=label)
    canvas.save(output)

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--graph',default='../work/assets/de_dust2.graph')
    parser.add_argument('--output',default='bridge/config/de_dust2.areas.json');parser.add_argument('--render');parser.add_argument('--overview',default='../work/runtime/Half-Life/cstrike/overviews/de_dust2.bmp')
    args=parser.parse_args();result,nodes=build(args.graph)
    Path(args.output).write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    if args.render:render(result,nodes,args.overview,args.render)
    print(json.dumps({'areas':len(result['areas']),'areas_with_nodes':sum(bool(a['waypoint_ids'])for a in result['areas']),
      'waypoints':len(nodes),'covered':result['covered_nodes'],'unmapped':len(result['unmapped_waypoints'])}))
