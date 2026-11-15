import json
import struct as S
from pathlib import Path
from plugin_records import sub,record
from plugin_records import emit_plugin,group

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'build/enhancements/Data'
SETTINGS=json.loads((ROOT/'config/enhancements.json').read_text())['settings']


def main(unified=False,script_only=False):
    OUT.mkdir(parents=True,exist_ok=True)
    settings=[dict(s,form_id=s['form_id']+(0x120 if unified else 0)) for s in SETTINGS]
    plugin='AstralSigil.esp' if unified else 'AstralSigil_MCM.esp'
    prefix=0x01000000 if unified else 0x02000000
    quest_id=prefix+(0x920 if unified else 0x800)
    header=['#pragma once','#include <array>','namespace Settings {','enum Key { '+', '.join(s['key'] for s in SETTINGS)+', Count };',
        'inline constexpr std::array<float,Count> Defaults{'+','.join(str(float(s['default']))+'f' for s in SETTINGS)+'};',
        'inline constexpr std::array<float,Count> Minimum{'+','.join(str(float(s.get('min',0)))+'f' for s in SETTINGS)+'};',
        'inline constexpr std::array<float,Count> Maximum{'+','.join(str(float(s.get('max',1)))+'f' for s in SETTINGS)+'};',
        'inline constexpr std::array<unsigned int,Count> FormIDs{'+','.join(hex(s['form_id']) for s in settings)+'};',
        'inline constexpr char ModuleName[]="'+plugin+'";','}']
    (ROOT/'native/Settings').mkdir(exist_ok=True)
    if not script_only:
        (ROOT/'native/Settings/Schema.h').write_text('\n'.join(header)+'\n')
    n=len(SETTINGS)
    lines=['Scriptname AS_MCM extends SKI_ConfigBase','Int[] _ids','Int[] _forms','String[] _labels','String[] _groups','String[] _descriptions','Bool[] _visible','Float[] _defaults','Float[] _minimum','Float[] _maximum','Float[] _steps','Bool[] _toggles','Int _reset','Int _dim','Int _standard','Int _bright',
        'Int Function GetVersion()',f'    Return {7 if unified else 3}','EndFunction','Event OnVersionUpdate(Int version)','    Setup()','EndEvent','Function Setup()', '    ModName = "Astral Sigil"','    Pages = new String[6]']
    pages=list(dict.fromkeys(s['page'] for s in SETTINGS))
    lines += [f'    Pages[{i}] = "{p}"' for i,p in enumerate(pages)]
    for key,kind in (('_ids','Int'),('_forms','Int'),('_labels','String'),('_groups','String'),('_descriptions','String'),('_visible','Bool'),('_defaults','Float'),('_minimum','Float'),('_maximum','Float'),('_steps','Float'),('_toggles','Bool')):lines.append(f'    {key} = new {kind}[{n}]')
    for i,s in enumerate(settings):
        lines += [f'    _forms[{i}] = {s["form_id"]}',f'    _labels[{i}] = "{s["label"]}"',f'    _groups[{i}] = "{s["page"]}"',f'    _defaults[{i}] = {float(s["default"])}',f'    _minimum[{i}] = {float(s.get("min",0))}',f'    _maximum[{i}] = {float(s.get("max",1))}',f'    _steps[{i}] = {float(s.get("step",1))}',f'    _toggles[{i}] = {"True" if s.get("type")=="toggle" else "False"}']
        lines += [f'    _visible[{i}] = {"True" if s.get("mcm_visible",True) else "False"}',
                  f'    _descriptions[{i}] = {json.dumps(s["description"])}']
    # Keep every existing GLOB and array slot stable; retire UI controls only.
    # Existing saves may have disabled these effects. Restore their enabled
    # visual default instead of retaining an inaccessible OFF state.
    for i,s in enumerate(settings):
        if not s.get('mcm_visible',True):
            lines += [f'    GlobalVariable retired{i} = Value({i})',f'    If retired{i} != None',
                      f'        retired{i}.SetValue({float(s["default"])})','    EndIf']
    lines += ['EndFunction','Event OnConfigInit()','    Setup()','EndEvent',
        'GlobalVariable Function Value(Int index)',f'    Return Game.GetFormFromFile(_forms[index], "{plugin}") as GlobalVariable','EndFunction',
        'Int Function FindOption(Int option)','    Int i = 0','    While i < _ids.Length','        If _ids[i] == option','            Return i','        EndIf','        i += 1','    EndWhile','    Return -1','EndFunction',
        'Event OnPageReset(String page)','    If _ids == None || _visible == None || _descriptions == None','        Setup()','    EndIf','    If page == ""','        page = "Visuals"','    EndIf','    _dim = -1','    _standard = -1','    _bright = -1','    SetCursorFillMode(TOP_TO_BOTTOM)','    AddHeaderOption(page)','    If page == "Visuals"','        _dim = AddTextOption("Brightness preset", "Dim")','        _standard = AddTextOption("Brightness preset", "Standard")','        _bright = AddTextOption("Brightness preset", "Bright")','    EndIf','    Int i = 0','    While i < _ids.Length','        _ids[i] = -1','        If _groups[i] == page && _visible[i]','            If _forms[i] == 2372','                _ids[i] = AddKeyMapOption(_labels[i], Value(i).GetValue() as Int)','            ElseIf _toggles[i]','                _ids[i] = AddToggleOption(_labels[i], Value(i).GetValue() > 0.5)','            Else','                _ids[i] = AddSliderOption(_labels[i], Value(i).GetValue(), "{1}")','            EndIf','        EndIf','        i += 1','    EndWhile','    AddEmptyOption()','    _reset = AddTextOption("Reset all settings", "Defaults")','EndEvent',
        'Event OnOptionSelect(Int option)','    If option == _dim || option == _standard || option == _bright','        Float level = 1.0','        If option == _dim','            level = 0.55','        ElseIf option == _bright','            level = 1.5','        EndIf','        Value(0).SetValue(level)','        ForcePageReset()','        Return','    EndIf','    If option == _reset','        Int j = 0','        While j < _defaults.Length','            Value(j).SetValue(_defaults[j])','            j += 1','        EndWhile','        ForcePageReset()','        Return','    EndIf','    Int i = FindOption(option)','    If i >= 0 && _toggles[i]','        Bool enabled = Value(i).GetValue() < 0.5','        If enabled','            Value(i).SetValue(1.0)','        Else','            Value(i).SetValue(0.0)','        EndIf','        SetToggleOptionValue(option, enabled)','    EndIf','EndEvent',
        'Event OnOptionSliderOpen(Int option)','    Int i = FindOption(option)','    If i >= 0','        SetSliderDialogStartValue(Value(i).GetValue())','        SetSliderDialogDefaultValue(_defaults[i])','        SetSliderDialogRange(_minimum[i], _maximum[i])','        SetSliderDialogInterval(_steps[i])','    EndIf','EndEvent',
        'Event OnOptionSliderAccept(Int option, Float amount)','    Int i = FindOption(option)','    If i >= 0','        Value(i).SetValue(amount)','        SetSliderOptionValue(option, amount, "{1}")','    EndIf','EndEvent',
        'Event OnOptionDefault(Int option)','    Int i = FindOption(option)','    If i >= 0','        Value(i).SetValue(_defaults[i])','        If _forms[i] == 2372','            SetKeyMapOptionValue(option, _defaults[i] as Int)','        ElseIf _toggles[i]','            SetToggleOptionValue(option, _defaults[i] > 0.5)','        Else','            SetSliderOptionValue(option, _defaults[i], "{1}")','        EndIf','    EndIf','EndEvent',
        'Event OnOptionHighlight(Int option)', '    If option == _reset', '        SetInfoText("Restores all settings to defaults, including G for transformation and extra ground fire damage OFF.")', '        Return', '    EndIf', '    If option == _dim || option == _standard || option == _bright', '        SetInfoText("Changes only Magic effect brightness: Dim 0.55, Standard 1.0, Bright 1.5. Other visual settings are unchanged.")', '        Return', '    EndIf', '    Int i = FindOption(option)', '    If i >= 0', '        SetInfoText(_descriptions[i])', '    Else', '        SetInfoText("Settings are saved with this character.")', '    EndIf', 'EndEvent']
    lines += [
        'Event OnOptionKeyMapChange(Int option, Int keyCode, String conflictControl, String conflictName)','    Int i = FindOption(option)','    If i < 0 || _forms[i] != 2372','        Return','    EndIf','    If keyCode > 255','        ShowMessage("Choose a keyboard key.", False)','        Return','    EndIf','    If conflictControl != "" || conflictName != ""','        If !ShowMessage("This key is already assigned. Use it anyway?")','            Return','        EndIf','    EndIf','    If keyCode < 0','        keyCode = 0','    EndIf','    Value(i).SetValue(keyCode)','    SetKeyMapOptionValue(option, keyCode)','EndEvent']
    (ROOT/'scripts/source/AS_MCM.psc').write_text('\n\n'.join(lines)+'\n')
    if script_only:
        print('Generated MCM v7 script:',sum(s.get('mcm_visible',True) for s in settings),'visible controls; native schema and plugin unchanged')
        return
    globals=[]
    for i,s in enumerate(settings):
        globals.append(record('GLOB',prefix+s['form_id'],sub('EDID',f'AS_Setting_{s["key"]}\0'.encode())+sub('FNAM',b'f')+sub('FLTV',S.pack('<f',s['default']))))
    if unified:
        globals.append(record('GLOB',0x01000945,sub('EDID',b'AS_GripTransitionState\0')+sub('FNAM',b'f')+sub('FLTV',S.pack('<f',0))))
    def st(value):v=value.encode();return S.pack('<H',len(v))+v
    vmad=S.pack('<HHH',5,2,1)+st('AS_MCM')+b'\0'+S.pack('<H',1)+st('ModName')+bytes((2,1))+st('Astral Sigil')
    vmad+=S.pack('<bH',2,0)+st('')+S.pack('<H',1)
    vmad+=S.pack('<HHIHHH',0,0,quest_id,5,2,1)+st('SKI_PlayerLoadGameAlias')+S.pack('<BH',0,0)
    quest=sub('EDID',b'AS_Configuration\0')+sub('VMAD',vmad)+sub('FULL',b'Astral Sigil Configuration\0')
    quest+=sub('DNAM',S.pack('<HBBII',0x1|0x100,0,0,0,0))+sub('NEXT',b'')+sub('ANAM',S.pack('<I',1))
    quest+=sub('ALST',S.pack('<I',0))+sub('ALID',b'PlayerAlias\0')+sub('FNAM',bytes(4))+sub('ALFR',S.pack('<I',0x14))+sub('VTCK',bytes(4))+sub('ALED',b'')
    target=ROOT/'build/unified/mcm-records.esp' if unified else OUT/'AstralSigil_MCM.esp'
    target.parent.mkdir(parents=True,exist_ok=True)
    extra_groups=[]
    if unified:
        from inspect_game import records,subrecords
        templates={r['form_id']:r for r in records(ROOT/'build/unified/Full/Data/Skyrim.esm',{'WEAP'}) if r['form_id'] in (0x139b0,0x139b2)}
        staves={r['form_id']:dict(subrecords(r['data'])) for r in records(ROOT/'release/inputs/base.esp',{'WEAP'}) if r['form_id'] in (0x01000800,0x01000820)}
        weapons=[];models=[]
        stat_templates={r['form_id']:r for r in records(ROOT/'release/inputs/base.esp',{'STAT'})}
        for variant,name in enumerate(('Stormheart','Cinderfang')):
            staff=staves[0x01000800+variant*0x20]
            for kind,(source,label,damage) in enumerate(((0x139b0,'Mace',17),)):
                template=templates[source];fields=[]
                for tag,value in subrecords(template['data']):
                    if tag=='EDID':value=f'AS_{name}_{label}\0'.encode()
                    elif tag=='FULL':value=f'{name} - {label}\0'.encode()
                    elif tag=='DESC':value=b'\0'
                    elif tag=='DATA':value=S.pack('<IfH',0,0,damage)
                    elif tag=='DNAM':
                        value=bytearray(value)
                        S.pack_into('<H',value,12,S.unpack_from('<H',value,12)[0]|0x88)
                        value=bytes(value)
                    elif tag=='MODL':value=f'astralsigil\\melee\\{name.lower()}_{label.lower()}_3rd.nif\0'.encode()
                    elif tag=='WNAM':value=S.pack('<I',0x01000946+variant*2+kind)
                    elif tag in ('MODT','MODS'):continue
                    fields.append(sub(tag,value))
                weapons.append(record('WEAP',0x01000940+variant*2+kind,b''.join(fields),4,template['version']))
                source_stat=stat_templates[S.unpack('<I',staff['WNAM'])[0]]
                model_fields=[]
                for tag,value in subrecords(source_stat['data']):
                    if tag=='EDID':value=f'AS_{name}_{label}_FirstPerson\0'.encode()
                    elif tag=='MODL':value=f'astralsigil\\melee\\{name.lower()}_{label.lower()}_1st.nif\0'.encode()
                    elif tag in ('MODT','MODS'):continue
                    model_fields.append(sub(tag,value))
                models.append(record('STAT',0x01000946+variant*2+kind,b''.join(model_fields),0,source_stat['version']))
        extra_groups=[group(b'WEAP',0,b''.join(weapons)),group(b'STAT',0,b''.join(models))]
    emit_plugin(target,['Skyrim.esm'] if unified else ['Skyrim.esm','AstralSigil.esp'],[group(b'GLOB',0,b''.join(globals)),group(b'QUST',0,record('QUST',quest_id,quest))]+extra_groups,'Astral Sigil settings for SkyUI MCM.',0x94a if unified else 0x816)
    if not unified:
        seq=OUT/'seq';seq.mkdir(exist_ok=True);(seq/'AstralSigil_MCM.seq').write_bytes(S.pack('<I',quest_id))
    print('Generated MCM quest,',n,'persistent settings, Papyrus and C++ schema')


if __name__=='__main__':
    import sys
    main('--unified' in sys.argv,'--script-only' in sys.argv)
