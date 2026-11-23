# Référence INDEPENDANTE du PBR (double précision), écrite d'après la spécification glTF 2.0
# (annexe B) : specular_brdf = F * D * Vis ; diffuse = (1 - F) * c_diff / pi ; l'éclairement du
# soleil est pi * sun (convention du moteur). Sert à produire les valeurs attendues des tests.
import math
PI=math.pi
def norm(v):
    n=math.sqrt(sum(x*x for x in v)); return [x/n for x in v] if n>1e-10 else v
def dot(a,b): return sum(x*y for x,y in zip(a,b))
def D(nh,a):
    a2=a*a; d=nh*nh*(a2-1)+1; return a2/(PI*d*d)
def V(nl,nv,a):
    a2=a*a
    return 0.5/(nl*math.sqrt(nv*nv*(1-a2)+a2)+nv*math.sqrt(nl*nl*(1-a2)+a2))
def F(f0,vh): return f0+(1-f0)*(1-max(0,min(1,vh)))**5
def envbrdf(nv,r):
    c0=[-1,-0.0275,-0.572,0.022]; c1=[1,0.0425,1.04,-0.04]
    rr=[r*c0[i]+c1[i] for i in range(4)]
    a004=min(rr[0]*rr[0],2**(-9.28*nv))*rr[0]+rr[1]
    return (-1.04*a004+rr[2], 1.04*a004+rr[3])
def envbrdf_lut(nv,r,n=1200):
    # (A, B) de la LUT BRDF par quadrature en (theta_h, phi_h) : voir ibl_ref.py (même méthode)
    import numpy as np
    a=r*r
    th=(np.arange(n)+0.5)/n*(np.pi/2); ph=(np.arange(2*n)+0.5)/(2*n)*2*np.pi
    T,P=np.meshgrid(th,ph,indexing='ij')
    H=np.stack([np.sin(T)*np.cos(P),np.sin(T)*np.sin(P),np.cos(T)],-1)
    Vv=np.array([math.sqrt(1-nv*nv),0,nv]); vh=H@Vv; L=2*vh[...,None]*H-Vv
    nl=L[...,2]; nh=H[...,2]; ok=(nl>0)&(vh>0); a2=a*a
    Dd=a2/(np.pi*(nh*nh*(a2-1)+1)**2)
    vis=0.5/(nl*np.sqrt(nv*nv*(1-a2)+a2)+nv*np.sqrt(nl*nl*(1-a2)+a2))
    fc=(1-vh)**5; w=np.sin(T)*(np.pi/2/n)*(2*np.pi/(2*n))*4*vh
    f=np.where(ok,Dd*vis*nl*w,0)
    return float((f*(1-fc)).sum()), float((f*fc).sum())
def shade(light,s,n,pos):
    n=norm(n); v=norm([light['cam'][i]-pos[i] for i in range(3)]); l=light['dir']
    h=norm([l[i]+v[i] for i in range(3)])
    rough=max(s['rough'],0.045); a=rough*rough
    nl=dot(n,l); nv=max(dot(n,v),1e-4); nh=max(dot(n,h),0); vh=max(dot(v,h),0)
    A,B=envbrdf_lut(nv,rough)   # M1-16 : LUT intégrée (environnement uniforme de radiance 1)
    out=[]
    for c in range(3):
        f0=0.04+(s['base'][c]-0.04)*s['metal']; cd=s['base'][c]*(1-s['metal'])
        fr=F(f0,vh); sun=light['sun'][c]; amb=light['amb'][c]
        direct=0
        if nl>0:
            direct=sun*nl*PI*((1-fr)*cd/PI+fr*D(nh,a)*V(nl,nv,a))
        ind=s['occ']*amb*(cd+f0*A+B)   # irradiance/pi = 1 et préfiltrée = 1
        out.append(direct+ind+s['emis'][c])
    return out
if __name__=='__main__':
    print('D(1,.5)',D(1,.5),'D(.5,.5)',D(.5,.5),'D(.3,1)',D(.3,1))
    print('V(1,1,.5)',V(1,1,.5),'V(.5,1,1)',V(.5,1,1),'V(.5,.25,.5)',V(.5,.25,.5))
    print('F',F(.04,1),F(.04,0),F(.04,.5),F(.8,.25))
    print('env(1,0)',envbrdf(1,0),'env(1,1)',envbrdf(1,1),'env(.5,.5)',envbrdf(.5,.5),'env(.1,.3)',envbrdf(.1,.3))
