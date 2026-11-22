# Intégration par FORCE BRUTE, sur chaque pixel de l'image équirectangulaire, de l'irradiance/pi et
# de la radiance filtrée par le lobe GGX (N = V = R) ; compare avec ce que calcule le moteur
# (test_ibl --filter=ibl_dump.*, voir README.md). Convention d'orientation de three.js :
# u = atan2(z, x) / 2pi + 0,5 ; v = asin(y) / pi + 0,5, première ligne de l'image = haut.
import numpy as np, sys, subprocess, os
hdr=sys.argv[1]; exe=sys.argv[2]
def load(path):  # lecteur Radiance (RLE) minimal, indépendant de stb
    f=open(path,'rb'); 
    while True:
        l=f.readline()
        if l.strip()==b'': break
    h=f.readline().split(); H=int(h[1]); W=int(h[3])
    img=np.zeros((H,W,4),np.uint8)
    for y in range(H):
        a=f.read(4); assert a[0]==2 and a[1]==2
        for c in range(4):
            x=0
            while x<W:
                n=f.read(1)[0]
                if n>128:
                    v=f.read(1)[0]; n-=128; img[y,x:x+n,c]=v; x+=n
                else:
                    img[y,x:x+n,c]=np.frombuffer(f.read(n),np.uint8); x+=n
    e=img[:,:,3].astype(np.int32)
    rgb=img[:,:,:3].astype(np.float64)*np.where(e>0,2.0**(e-136),0)[:,:,None]
    return rgb
E_=load(hdr); H,W,_=E_.shape
# direction de chaque pixel
us=(np.arange(W)+0.5)/W; vs=1-(np.arange(H)+0.5)/H
phi=(us-0.5)*2*np.pi; th=(vs-0.5)*np.pi
X=np.cos(th)[:,None]*np.cos(phi)[None,:]; Y=np.sin(th)[:,None]*np.ones(W)[None,:]; Z=np.cos(th)[:,None]*np.sin(phi)[None,:]
dirs=np.stack([X,Y,Z],-1).reshape(-1,3); L=E_.reshape(-1,3)
dw=(np.cos(th)[:,None]*np.ones(W)[None,:]*(np.pi/H)*(2*np.pi/W)).reshape(-1)
def norm(v): return v/np.linalg.norm(v)
def irr(n): c=np.clip(dirs@n,0,None); return (L*(c*dw)[:,None]).sum(0)/np.pi
def pre(n,r):
    a=r*r; a2=a*a; nl=np.clip(dirs@n,0,None)
    h=dirs+n; h/=np.linalg.norm(h,axis=1,keepdims=True); nh=np.clip(h@n,0,1)
    D=a2/(np.pi*(nh*nh*(a2-1)+1)**2); w=nl*D/4*dw   # pdf(L) = D/4 pour N = V ; poids N.L
    return (L*w[:,None]).sum(0)/w.sum()
# brillant : la direction du maximum de luminance
k=np.argmax(L.sum(1)); sun=dirs[k]
ds=[sun,[0,1,0],[1,0,0],[0,0,-1],[0,-1,0],norm(np.array([-1,0.3,0]))]
arg=';'.join('%f,%f,%f'%tuple(d) for d in ds)
env=dict(os.environ,OW_ENV=hdr,OW_DIRS=arg)
out=subprocess.run([exe,'--filter=ibl_dump.filtered_values_for_a_real_environment'],env=env,capture_output=True,text=True).stdout
eng={}
cur=None
for line in out.splitlines():
    if line.startswith('DIR'): cur=tuple(float(x) for x in line.split()[1:]); eng[cur]={}
    elif line.startswith('  ') and cur: p=line.split(); eng[cur][p[0]]=np.array([float(x) for x in p[1:]])
np.set_printoptions(precision=4,suppress=True)
for d,vals in eng.items():
    n=norm(np.array(d)); print('direction',np.round(n,3))
    print('  irradiance : moteur',vals['irr'],' force brute',irr(n))
    for l in range(1,4):
        r=l/3; print('  rugosité %.2f : moteur'%r,vals['pre%d'%l],' force brute',pre(n,r))
