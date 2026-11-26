# Escribe manifiesto.json para el sitio: versión, módulos y archivo por sistema
# uso: python3 manifiesto.py <carpeta con .vcvplugin> <plugin.json> <commit>
import json, os, sys, datetime
carpeta, pj, commit = sys.argv[1], sys.argv[2], (sys.argv[3] if len(sys.argv) > 3 else '')
p = json.load(open(pj, encoding='utf-8'))
archivos = {}
for f in sorted(os.listdir(carpeta)):
    if not f.endswith('.vcvplugin'): continue
    arch = f[:-len('.vcvplugin')].split('-')[-2:]
    archivos['-'.join(arch)] = {'archivo': f, 'bytes': os.path.getsize(os.path.join(carpeta, f))}
print(json.dumps({
    'slug': p['slug'], 'nombre': p['name'], 'version': p['version'], 'autor': p.get('author', ''), 'licencia': p.get('license', ''),
    'fecha': datetime.datetime.now(datetime.timezone.utc).strftime('%Y-%m-%d %H:%M UTC'), 'commit': commit[:7],
    'modulos': [{'slug': m['slug'], 'nombre': m['name'], 'descripcion': m.get('description', ''), 'etiquetas': m.get('tags', [])} for m in p['modules']],
    'archivos': archivos,
}, ensure_ascii=False, indent=2))
