# Compare engine_<n>.ppm (moteur) et three<mode>_<n>.rgba (three.js) du dossier donné en argument.
import numpy as np, sys
D=sys.argv[1] if len(sys.argv) > 1 else '.'
from PIL import Image
def eng(i): return np.asarray(Image.open(f'{D}/engine_{i}.ppm').convert('RGB'),dtype=np.float64)
def thr(m,i): return np.frombuffer(open(f'{D}/three{m}_{i}.rgba','rb').read(),dtype=np.uint8).reshape(512,512,4)[:,:,:3].astype(np.float64)
def srgb(a): return 255*(np.clip(a/255,0,1)**(1/2.2))
for m in 'ABCD':
    print('== three mode',m)
    for i in range(4):
        e,t=eng(i),thr(m,i)
        any_=(e.max(2)>0)|(t.max(2)>0)
        d=np.abs(e-t)
        dm=d.max(2)[any_]
        mse=((e-t)**2)[any_].mean()
        ds=np.abs(srgb(e)-srgb(t)).max(2)[any_]
        ratio=e[any_].sum()/t[any_].sum()
        print(i,'px',any_.sum(),'sum ratio eng/three %.4f'%ratio,'MAE %.2f'%d[any_].mean(),'p50 %d p90 %d p99 %d'%tuple(np.percentile(dm,[50,90,99])),
              '<=4: %.1f%% <=8: %.1f%%'%((dm<=4).mean()*100,(dm<=8).mean()*100),'PSNR(linear) %.1f dB'%(10*np.log10(255**2/mse)),'sRGB MAE %.2f'%ds.mean())
