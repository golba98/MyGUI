#!/usr/bin/env python3
"""Native X11/GL comparison. Requires Pillow, NumPy, X11 and XTest.
Outputs uniquely named captures plus metadata and trace, never a reused montage.
"""
import ctypes as C, ctypes.util, subprocess, time, os
from PIL import ImageGrab, Image, ImageDraw
x=C.CDLL(ctypes.util.find_library('X11')); t=C.CDLL(ctypes.util.find_library('Xtst'))
def bind(lib,name,rest,args):
    f=getattr(lib,name); f.restype=rest; f.argtypes=args; return f
P=C.c_void_p; W=C.c_ulong; I=C.c_int
open_display=bind(x,'XOpenDisplay',P,[C.c_char_p]); d=open_display(None); assert d
root=bind(x,'XDefaultRootWindow',W,[P])(d)
class XError(C.Structure):
    _fields_=[('type',I),('display',P),('resourceid',W),('serial',W),('error_code',C.c_ubyte),('request_code',C.c_ubyte),('minor_code',C.c_ubyte)]
xerrors=[]
@C.CFUNCTYPE(I,P,C.POINTER(XError))
def on_error(display,event):
    e=event.contents;xerrors.append(dict(resource=int(e.resourceid),error=e.error_code,request=e.request_code,minor=e.minor_code));return 0
bind(x,'XSetErrorHandler',P,[C.c_void_p])(C.cast(on_error,P))
query=bind(x,'XQueryTree',I,[P,W,C.POINTER(W),C.POINTER(W),C.POINTER(C.POINTER(W)),C.POINTER(C.c_uint)])
name=bind(x,'XFetchName',I,[P,W,C.POINTER(C.c_char_p)])
free=bind(x,'XFree',I,[P]); sync=bind(x,'XSync',I,[P,I])
focus=bind(x,'XSetInputFocus',I,[P,W,I,W]); move=bind(t,'XTestFakeMotionEvent',I,[P,I,I,I,W]); button=bind(t,'XTestFakeButtonEvent',I,[P,C.c_uint,I,W]); fakekey=bind(t,'XTestFakeKeyEvent',I,[P,C.c_uint,I,W])
keysym=bind(x,'XStringToKeysym',W,[C.c_char_p]); keycode=bind(x,'XKeysymToKeycode',C.c_ubyte,[P,W])

import numpy as np, json, argparse, pathlib, hashlib, re
class Visual(C.Structure):
    _fields_=[('ext_data',P),('visualid',W),('class_',I),('red_mask',W),('green_mask',W),('blue_mask',W),('bits_per_rgb',I),('map_entries',I)]
class Attributes(C.Structure):
    _fields_=[('x',I),('y',I),('width',I),('height',I),('border_width',I),('depth',I),('visual',C.POINTER(Visual)),('root',W),('class_',I),('bit_gravity',I),('win_gravity',I),('backing_store',I),('backing_planes',W),('backing_pixel',W),('save_under',I),('colormap',W),('map_installed',I),('map_state',I),('all_event_masks',C.c_long),('your_event_mask',C.c_long),('do_not_propagate_mask',C.c_long),('override_redirect',I),('screen',P)]
class XImage(C.Structure):
    _fields_=[('width',I),('height',I),('xoffset',I),('format',I),('data',P),('byte_order',I),('bitmap_unit',I),('bitmap_bit_order',I),('bitmap_pad',I),('depth',I),('bytes_per_line',I),('bits_per_pixel',I),('red_mask',W),('green_mask',W),('blue_mask',W)]
attrsfn=bind(x,'XGetWindowAttributes',I,[P,W,C.POINTER(Attributes)])
getimage=bind(x,'XGetImage',C.POINTER(XImage),[P,W,I,I,C.c_uint,C.c_uint,W,I])
destroy=bind(x,'XDestroyImage',I,[C.POINTER(XImage)])
translate=bind(x,'XTranslateCoordinates',I,[P,W,W,I,I,C.POINTER(I),C.POINTER(I),C.POINTER(W)])
def attrs(w):
    a=Attributes(); assert attrsfn(d,w,C.byref(a)); return a
def metadata(w):
    a=attrs(w); v=a.visual.contents
    return dict(id=int(w),width=a.width,height=a.height,depth=a.depth,map_state=a.map_state,visual=int(v.visualid),visual_class=v.class_,masks=[v.red_mask,v.green_mask,v.blue_mask])
image_formats=[]
def capture(w,px,py,width,height,path):
    im=getimage(d,w,px,py,width,height,C.c_ulong(-1).value,2)
    sync(d,0)
    if not im:raise RuntimeError('XGetImage failed: '+repr(xerrors))
    q=im.contents; assert q.bits_per_pixel==32
    image_format=dict(depth=q.depth,bits_per_pixel=q.bits_per_pixel,byte_order=q.byte_order,bytes_per_line=q.bytes_per_line,masks=[q.red_mask,q.green_mask,q.blue_mask])
    if image_format not in image_formats:image_formats.append(image_format)
    raw=C.string_at(q.data,q.bytes_per_line*q.height)
    pixels=np.frombuffer(raw,dtype='<u4' if q.byte_order==0 else '>u4').reshape(q.height,q.bytes_per_line//4)[:,:q.width]
    rgb=[]
    for mask in [q.red_mask,q.green_mask,q.blue_mask]:
        shift=(mask & -mask).bit_length()-1; rgb.append(((pixels & mask)>>shift)*255//(mask>>shift))
    result=Image.fromarray(np.stack(rgb,axis=2).astype('uint8')); result.save(path)
    destroy(im); return result

parser=argparse.ArgumentParser()
parser.add_argument('--program',required=True)
parser.add_argument('--out',required=True)
parser.add_argument('--continuous',action='store_true')
parser.add_argument('--control',action='store_true')
parser.add_argument('--readbacks',action='store_false',dest='no_readbacks',help='opt in to GL readbacks, which can perturb GLX presentation')
parser.add_argument('--no-readbacks',action='store_true',default=True)
parser.add_argument('--read-phase',default='all',choices=['all','before-swap-back','after-swap-front','idle-after-last-swap-front','idle-after-last-swap-back-undefined'])
parser.add_argument('--order',choices=['client','root','pillow'],default='root')
args=parser.parse_args()
out=pathlib.Path(args.out).resolve()
if out.exists() and any(out.iterdir()):parser.error('use a fresh output directory; captures must not overwrite previous evidence')
out.mkdir(parents=True,exist_ok=True)
trace=out/'trace.log'
env=os.environ.copy(); env['MYGUI_PRESENT_TRACE']='1'
if not args.no_readbacks: env['MYGUI_PRESENT_CAPTURE_DIR']=str(out/'gl')
env['MYGUI_PRESENT_READ_PHASE']=args.read_phase
argv=[str(pathlib.Path(args.program).resolve())]
if args.continuous: argv+=['--continuous'] if args.control else ['--frames','1000000']
logfile=trace.open('w')
proc=subprocess.Popen(argv,env=env,stderr=logfile)
previous=W(); revert=I(); bind(x,'XGetInputFocus',I,[P,C.POINTER(W),C.POINTER(I)])(d,C.byref(previous),C.byref(revert))
report=dict(read_phase=args.read_phase,program=argv,display=os.environ['DISPLAY'],mode='continuous' if args.continuous else 'demand',readbacks=not args.no_readbacks,order=args.order,captures=[])
repo=pathlib.Path(__file__).resolve().parents[2]
report['revision']=subprocess.check_output(['git','rev-parse','HEAD'],cwd=repo,text=True).strip()
report['source_sha256']={p:hashlib.sha256((repo/p).read_bytes()).hexdigest() for p in ['src/UIContext.cpp','src/Window.cpp','src/Renderer.cpp','examples/common/Example.hpp','examples/combobox.cpp']}
report['x_server']=dict(vendor=bind(x,'XServerVendor',C.c_char_p,[P])(d).decode(),release=bind(x,'XVendorRelease',I,[P])(d))
report['image_formats']=image_formats
window=None
try:
    for _ in range(200):
        assert proc.poll() is None,'application exited before mapping'
        native_id=re.search(r'ENV native_client=(\d+)',trace.read_text())
        if native_id:window=int(native_id[1])
        if window and attrs(window).map_state==2: break
        time.sleep(.025)
    assert window and attrs(window).map_state==2,'no mapped client window'
    report['root']=metadata(root);report['client']=metadata(window)
    focus(d,window,1,0);sync(d,0)
    def text(): return trace.read_text()
    def until(predicate):
        deadline=time.monotonic()+5
        while not predicate():
            assert proc.poll() is None,'application exited during observation'
            assert time.monotonic()<deadline,'timed out waiting for diagnostic acknowledgement'
            time.sleep(.01)
    def location():
        px=I();py=I();child=W();assert translate(d,window,root,0,0,C.byref(px),C.byref(py),C.byref(child));return px.value,py.value
    def settle():
        # Bounded observation delay, not an application repaint/workaround.
        time.sleep(.15)
    def click(px,py,b=1):
        ox,oy=location();move(d,-1,ox+px,oy+py,0);button(d,b,1,0);button(d,b,0,0);sync(d,0);settle()
    def key(s):
        c=keycode(d,keysym(s.encode()));fakekey(d,c,1,0);fakekey(d,c,0,0);sync(d,0);settle()
    def count(tag):return sum(tag in line for line in text().splitlines())
    def snapshot(label):
        settle();assert proc.poll() is None
        a=attrs(window);assert a.map_state==2 and a.width>0 and a.height>0
        ox,oy=location()
        glfront=None
        if not args.no_readbacks:
            before=count('capture-command-done');key('F12');until(lambda:count('capture-command-done')>before)
            files=list((out/'gl').glob('*-idle-after-last-swap-front.ppm'))
            if files:glfront=max(files,key=lambda p:int(p.name.split('-')[0]))
        print('DRAWABLE',label,json.dumps(dict(client=metadata(window),root_xy=[ox,oy],root=metadata(root))),flush=True)
        start=time.monotonic_ns();refresh_before=count('event 12 ')+count('control-refresh')
        images={}
        order=['client','root','pillow'];order.remove(args.order);order.insert(0,args.order)
        for kind in order:
            path=out/(label+'-'+kind+'.png')
            if kind=='pillow':
                im=ImageGrab.grab(xdisplay=os.environ['DISPLAY']).crop((ox,oy,ox+a.width,oy+a.height)).convert('RGB');im.save(path)
            else:im=capture(window if kind=='client' else root,0 if kind=='client' else ox,0 if kind=='client' else oy,a.width,a.height,path)
            images[kind]=im
        settle()
        entry=dict(label=label,time_ns=start,pid=proc.pid,alive=proc.poll() is None,client=metadata(window),root_xy=[ox,oy],refresh_during_capture=count('event 12 ')+count('control-refresh')-refresh_before,swaps=count('swap-return'))
        entry['native']={k:dict(hash=hashlib.sha256(v.tobytes()).hexdigest(),marker=list(v.getpixel((10,a.height-16))),nonblack=int(np.count_nonzero(np.array(v)))) for k,v in images.items()}
        entry['client_root_equal']=images['client'].tobytes()==images['root'].tobytes()
        entry['client_pillow_equal']=images['client'].tobytes()==images['pillow'].tobytes()
        if glfront:
            gl=Image.open(glfront).convert('RGB');entry['gl_front']=str(glfront.relative_to(out));entry['client_gl_equal']=images['client'].tobytes()==gl.tobytes()
        backfiles=list((out/'gl').glob('*-before-swap-back.ppm'))
        if backfiles:
            back=max(backfiles,key=lambda p:int(p.name.split('-')[0]))
            entry['last_rendered_back']=str(back.relative_to(out));entry['client_last_back_equal']=images['client'].tobytes()==Image.open(back).convert('RGB').tobytes()
        report['captures'].append(entry)
        if args.no_readbacks:
            assert entry['client_root_equal'] and entry['client_pillow_equal'],'native capture methods disagree'
            assert entry['native']['client']['marker']==[255,0,255],'presented marker missing or stale'
        print(label,'swaps',entry['swaps'],'client/root',entry['client_root_equal'],'client/Pillow',entry['client_pillow_equal'],'client/GL',entry.get('client_gl_equal'),'marker',entry['native']['client']['marker'],flush=True)
    until(lambda:count('swap-return')>0)
    snapshot('01-initial')
    if not args.control:
        click(100,65);snapshot('02-open');key('Down');snapshot('03-highlight');key('Return');snapshot('04-commit')
        click(100,65);click(610,350);snapshot('05-outside-dismiss')
        click(100,65);key('Escape');snapshot('06-cancel')
        click(100,110);click(100,200,5);snapshot('07-scroll');key('Escape')
        click(100,155);key('Down');snapshot('08-modal-popup');key('Escape');snapshot('09-modal-without-popup');key('Escape')
        click(100,65);focus(d,root,1,0);sync(d,0);snapshot('10-focus-lost');focus(d,window,1,0);sync(d,0);snapshot('11-focus-return')
    else:key('space');snapshot('02-key-redraw')
    bind(x,'XResizeWindow',I,[P,W,C.c_uint,C.c_uint])(d,window,720,560);sync(d,0);snapshot('12-resize')
    # Cover only this test window; destroy the cover before any observation.
    ox,oy=location();cover=bind(x,'XCreateSimpleWindow',W,[P,W,I,I,C.c_uint,C.c_uint,C.c_uint,W,W])(d,root,ox,oy,100,100,0,0,0xffffff)
    bind(x,'XMapRaised',I,[P,W])(d,cover);sync(d,0);settle();bind(x,'XDestroyWindow',I,[P,W])(d,cover);sync(d,0);snapshot('13-uncover')
    bind(x,'XUnmapWindow',I,[P,W])(d,window);sync(d,0);settle();bind(x,'XMapWindow',I,[P,W])(d,window);sync(d,0)
    until(lambda:attrs(window).map_state==2);focus(d,window,1,0);sync(d,0);snapshot('14-remap')
    before=count('swap-return');refresh=count('event 12 ')+count('control-refresh');time.sleep(.5)
    report['idle']=dict(seconds=.5,swaps=count('swap-return')-before,refresh=count('event 12 ')+count('control-refresh')-refresh)
    snapshot('15-idle')
    report['trace_lines']=len(text().splitlines())
    report['environment']=[line for line in text().splitlines() if line.startswith('ENV ')]
finally:
    report['environment']=[line for line in trace.read_text().splitlines() if line.startswith('ENV ')]
    report['alive_before_cleanup']=proc.poll() is None
    report['x_errors']=xerrors
    if proc.poll() is None:proc.terminate()
    proc.wait(timeout=5);logfile.close()
    (out/'report.json').write_text(json.dumps(report,indent=2)+'\n')
    if previous.value:focus(d,previous.value,revert.value,0);sync(d,0)
    bind(x,'XCloseDisplay',I,[P])(d)
print('REPORT',out/'report.json','idle',report.get('idle'))
