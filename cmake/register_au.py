import plistlib,sys,copy
from pathlib import Path
p=Path(sys.argv[1]);data=plistlib.loads(p.read_bytes())
entry=data['AudioComponents'][0]
items=[]
for model in (286,376):
    item=copy.deepcopy(entry)
    item['name']=f'Frozz: dLab {model}'
    item['subtype']=f'C{model}'
    items.append(item)
data['AudioComponents']=items
p.write_bytes(plistlib.dumps(data))
