import json,sys
from pathlib import Path
p=Path(sys.argv[1])
classes=[]
for model in (286,376):
 for category,prefix in [('Audio Module Class','ABCDEF019182FAEB'),('Component Controller Class','ABCDEF011234ABCD')]:
  classes.append({'CID':prefix+'46727A7A'+f'C{model}'.encode().hex().upper(),'Category':category,'Name':f'dLab {model}','Vendor':'Frozz','Version':'1.0.0','SDKVersion':'VST 3.8.0','Sub Categories':['Fx'],'Class Flags':0,'Cardinality':2147483647,'Snapshots':[]})
p.write_text(json.dumps({'Name':'dLab.Series','Version':'1.0.0','Factory Info':{'Vendor':'Frozz','URL':'','E-Mail':'','Flags':{'Unicode':True,'Classes Discardable':False,'Component Non Discardable':False}},'Classes':classes},indent=2))
