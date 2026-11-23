import numpy as np
# BRDF LUT (A,B) par quadrature en (theta_h, phi_h) sur grille uniforme, indépendante du Monte-Carlo C
def lut(nv, r, n=1500):
    a=r*r
    th=(np.arange(n)+0.5)/n*(np.pi/2); ph=(np.arange(2*n)+0.5)/(2*n)*2*np.pi
    T,P=np.meshgrid(th,ph,indexing='ij')
    H=np.stack([np.sin(T)*np.cos(P),np.sin(T)*np.sin(P),np.cos(T)],-1)
    V=np.array([np.sqrt(1-nv*nv),0,nv])
    vh=H@V; L=2*vh[...,None]*H-V
    nl=L[...,2]; nh=H[...,2]
    ok=(nl>0)&(vh>0)
    a2=a*a
    D=a2/(np.pi*(nh*nh*(a2-1)+1)**2)
    vis=0.5/(nl*np.sqrt(nv*nv*(1-a2)+a2)+nv*np.sqrt(nl*nl*(1-a2)+a2))
    # dω_L = 4 vh dω_H ; spéculaire = D vis nl ; Fresnel Schlick
    fc=(1-vh)**5
    w=np.sin(T)*(np.pi/2/n)*(2*np.pi/(2*n))*4*vh
    f=np.where(ok,D*vis*nl*w,0)
    return (f*(1-fc)).sum(), (f*fc).sum()
for nv,r in [(0.5,0.5),(0.9,0.2),(0.2,0.8),(0.75,1.0),(0.1,0.3)]:
    print("lut",nv,r,"%.5f %.5f"%lut(nv,r))
# préfiltre : env L=1+y, N=(0,1,0), poids N·L, échantillons ~ GGX : k = ∫D (N·H) ... évalué par quadrature en L
def pre(r,n=3000):
    a=r*r; a2=a*a
    th=(np.arange(n)+0.5)/n*(np.pi/2); ph=(np.arange(2*n)+0.5)/(2*n)*2*np.pi
    T,P=np.meshgrid(th,ph,indexing='ij')
    # quadrature sur L (N=V=up) : poids D(H) pdf ... pondération N·L, densité de proba = D N·H/(4 V·H) sur L ; l'estimateur
    # E[L_y N·L]/E[N·L] sous cette densité = ∫ Ly nl pdf dωL / ∫ nl pdf dωL
    Lv=np.stack([np.sin(T)*np.cos(P),np.sin(T)*np.sin(P),np.cos(T)],-1)  # axe z = N
    nl=Lv[...,2]
    H=Lv+np.array([0,0,1.0]); H/=np.linalg.norm(H,axis=-1,keepdims=True)
    nh=H[...,2]
    D=a2/(np.pi*(nh*nh*(a2-1)+1)**2)
    pdf=D/4  # N=V
    w=np.sin(T)*(np.pi/2/n)*(2*np.pi/(2*n))
    num=(nl*nl*pdf*w).sum(); den=(nl*pdf*w).sum()
    return num/den  # = coefficient de y : env filtrée(N=up) = 1 + k
for r in [0.25,0.5,0.75,1.0]:
    print("pre",r,"%.5f"%pre(r))
