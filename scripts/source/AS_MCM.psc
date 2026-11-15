Scriptname AS_MCM extends SKI_ConfigBase

Int[] _ids

Int[] _forms

String[] _labels

String[] _groups

String[] _descriptions

Bool[] _visible

Float[] _defaults

Float[] _minimum

Float[] _maximum

Float[] _steps

Bool[] _toggles

Int _reset

Int _dim

Int _standard

Int _bright

Int Function GetVersion()

    Return 7

EndFunction

Event OnVersionUpdate(Int version)

    Setup()

EndEvent

Function Setup()

    ModName = "Astral Sigil"

    Pages = new String[6]

    Pages[0] = "Visuals"

    Pages[1] = "Casting"

    Pages[2] = "Projectile"

    Pages[3] = "Ground effects"

    Pages[4] = "Combat"

    Pages[5] = "Transformation"

    _ids = new Int[16]

    _forms = new Int[16]

    _labels = new String[16]

    _groups = new String[16]

    _descriptions = new String[16]

    _visible = new Bool[16]

    _defaults = new Float[16]

    _minimum = new Float[16]

    _maximum = new Float[16]

    _steps = new Float[16]

    _toggles = new Bool[16]

    _forms[0] = 2337

    _labels[0] = "Magic effect brightness"

    _groups[0] = "Visuals"

    _defaults[0] = 1.0

    _minimum[0] = 0.25

    _maximum[0] = 2.0

    _steps[0] = 0.05

    _toggles[0] = False

    _visible[0] = True

    _descriptions[0] = "Scales the glow of casting effects, projectiles and tracked impact/ground effects. Default: 1.0. Does not change scene lighting or the staff metal."

    _forms[1] = 2338

    _labels[1] = "First-person casting size"

    _groups[1] = "Visuals"

    _defaults[1] = 0.8

    _minimum[1] = 0.5

    _maximum[1] = 1.2

    _steps[1] = 0.05

    _toggles[1] = False

    _visible[1] = True

    _descriptions[1] = "Scales the rune circle and held spear in first person only. Default: 0.8. Does not resize the staff, third-person effect or flying projectile."

    _forms[2] = 2339

    _labels[2] = "Casting core and seal glow"

    _groups[2] = "Visuals"

    _defaults[2] = 0.45

    _minimum[2] = 0.1

    _maximum[2] = 1.0

    _steps[2] = 0.05

    _toggles[2] = False

    _visible[2] = True

    _descriptions[2] = "Adjusts the central light core and central seal of the casting circle. Default: 0.45. Multiplies Magic effect brightness; does not control the entire spear or staff crystal."

    _forms[3] = 2340

    _labels[3] = "Ready-state rune size pulse"

    _groups[3] = "Casting"

    _defaults[3] = 1.0

    _minimum[3] = 0.0

    _maximum[3] = 1.0

    _steps[3] = 1.0

    _toggles[3] = True

    _visible[3] = True

    _descriptions[3] = "Adds a subtle size pulse to the rune circle while holding a fully charged cast after four seconds. Does not disable glow pulses built into the effect."

    _forms[4] = 2341

    _labels[4] = "Final spear compression"

    _groups[4] = "Casting"

    _defaults[4] = 1.0

    _minimum[4] = 0.0

    _maximum[4] = 1.0

    _steps[4] = 1.0

    _toggles[4] = True

    _visible[4] = True

    _descriptions[4] = "Adds a brief 12 percent compression of the held spear near the end of the four-second charge. Does not disable crystal assembly or change charge time."

    _forms[5] = 2342

    _labels[5] = "Rune-circle kickback"

    _groups[5] = "Casting"

    _defaults[5] = 1.0

    _minimum[5] = 0.0

    _maximum[5] = 1.0

    _steps[5] = 1.0

    _toggles[5] = True

    _visible[5] = True

    _descriptions[5] = "Adds a short backward movement of the rune circle when a shot is released. Does not change character recoil, projectile direction or damage."

    _forms[6] = 2343

    _labels[6] = "Crystal scatter on cancellation"

    _groups[6] = "Casting"

    _defaults[6] = 1.0

    _minimum[6] = 0.0

    _maximum[6] = 1.0

    _steps[6] = 1.0

    _toggles[6] = True

    _visible[6] = False

    _descriptions[6] = "Retired control: partial material-sharing conflict on Cinderfang. Restored to the enabled visual default during menu initialization/update."

    _forms[7] = 2346

    _labels[7] = "Embers and ice fragments in flight"

    _groups[7] = "Projectile"

    _defaults[7] = 1.0

    _minimum[7] = 0.0

    _maximum[7] = 1.0

    _steps[7] = 1.0

    _toggles[7] = True

    _visible[7] = False

    _descriptions[7] = "Retired control: partial material-sharing conflict on Cinderfang. Restored to the enabled visual default during menu initialization/update."

    _forms[8] = 2347

    _labels[8] = "Broken tracer arcs"

    _groups[8] = "Projectile"

    _defaults[8] = 1.0

    _minimum[8] = 0.0

    _maximum[8] = 1.0

    _steps[8] = 1.0

    _toggles[8] = True

    _visible[8] = True

    _descriptions[8] = "ON shows gaps in the tracer rings around the flying projectile. OFF lights the gap pieces for fuller rings. Does not disable all tracer rings."

    _forms[9] = 2348

    _labels[9] = "Embers at fire impact"

    _groups[9] = "Ground effects"

    _defaults[9] = 1.0

    _minimum[9] = 0.0

    _maximum[9] = 1.0

    _steps[9] = 1.0

    _toggles[9] = True

    _visible[9] = True

    _descriptions[9] = "Shows small embers in Cinderfang's impact burst. Does not control the flying projectile, ground fire or impact damage."

    _forms[10] = 2349

    _labels[10] = "Ground ice crystals"

    _groups[10] = "Ground effects"

    _defaults[10] = 1.0

    _minimum[10] = 0.0

    _maximum[10] = 1.0

    _steps[10] = 1.0

    _toggles[10] = True

    _visible[10] = True

    _descriptions[10] = "Shows the growing crystals in Stormheart's ground residue. Does not control the held spear or crystals on the flying projectile."

    _forms[11] = 2350

    _labels[11] = "New ground effect lifetime"

    _groups[11] = "Ground effects"

    _defaults[11] = 5.0

    _minimum[11] = 3.0

    _maximum[11] = 8.0

    _steps[11] = 1.0

    _toggles[11] = False

    _visible[11] = True

    _descriptions[11] = "Lifetime of newly created fire and ice ground effects: 3-8 seconds; default 5. Existing effects keep their own lifetime. Does not change scorch decals or explosion duration."

    _forms[12] = 2353

    _labels[12] = "Ice crackle on new ground effects"

    _groups[12] = "Ground effects"

    _defaults[12] = 1.0

    _minimum[12] = 0.0

    _maximum[12] = 1.0

    _steps[12] = 1.0

    _toggles[12] = True

    _visible[12] = True

    _descriptions[12] = "Enables crackling sound on newly created ice ground effects. Changing this does not stop a loop already playing on an existing effect."

    _forms[13] = 2354

    _labels[13] = "Ground fire deals damage"

    _groups[13] = "Combat"

    _defaults[13] = 0.0

    _minimum[13] = 0.0

    _maximum[13] = 1.0

    _steps[13] = 1.0

    _toggles[13] = True

    _visible[13] = True

    _descriptions[13] = "Adds approximately one damage application per second to hostile living enemies near Cinderfang's ground fire. Excludes the player and companions; overlapping patches do not stack this extra hit. OFF by default."

    _forms[14] = 2355

    _labels[14] = "Ground fire damage per tick"

    _groups[14] = "Combat"

    _defaults[14] = 3.0

    _minimum[14] = 1.0

    _maximum[14] = 10.0

    _steps[14] = 1.0

    _toggles[14] = False

    _visible[14] = True

    _descriptions[14] = "Base damage for each ground-fire tick, approximately once per second. Only used when Ground fire deals damage is ON. Default: 3. Actual damage depends on game effect processing and resistances."

    _forms[15] = 2372

    _labels[15] = "Transform weapon"

    _groups[15] = "Transformation"

    _defaults[15] = 34.0

    _minimum[15] = 0.0

    _maximum[15] = 255.0

    _steps[15] = 1.0

    _toggles[15] = False

    _visible[15] = True

    _descriptions[15] = "Keyboard key for switching a drawn Stormheart or Cinderfang between staff and mace. Default: G. Unmap to disable. Unavailable while casting, attacking, in menus or other blocked states."

    GlobalVariable retired6 = Value(6)

    If retired6 != None

        retired6.SetValue(1.0)

    EndIf

    GlobalVariable retired7 = Value(7)

    If retired7 != None

        retired7.SetValue(1.0)

    EndIf

EndFunction

Event OnConfigInit()

    Setup()

EndEvent

GlobalVariable Function Value(Int index)

    Return Game.GetFormFromFile(_forms[index], "AstralSigil.esp") as GlobalVariable

EndFunction

Int Function FindOption(Int option)

    Int i = 0

    While i < _ids.Length

        If _ids[i] == option

            Return i

        EndIf

        i += 1

    EndWhile

    Return -1

EndFunction

Event OnPageReset(String page)

    If _ids == None || _visible == None || _descriptions == None

        Setup()

    EndIf

    If page == ""

        page = "Visuals"

    EndIf

    _dim = -1

    _standard = -1

    _bright = -1

    SetCursorFillMode(TOP_TO_BOTTOM)

    AddHeaderOption(page)

    If page == "Visuals"

        _dim = AddTextOption("Brightness preset", "Dim")

        _standard = AddTextOption("Brightness preset", "Standard")

        _bright = AddTextOption("Brightness preset", "Bright")

    EndIf

    Int i = 0

    While i < _ids.Length

        _ids[i] = -1

        If _groups[i] == page && _visible[i]

            If _forms[i] == 2372

                _ids[i] = AddKeyMapOption(_labels[i], Value(i).GetValue() as Int)

            ElseIf _toggles[i]

                _ids[i] = AddToggleOption(_labels[i], Value(i).GetValue() > 0.5)

            Else

                _ids[i] = AddSliderOption(_labels[i], Value(i).GetValue(), "{1}")

            EndIf

        EndIf

        i += 1

    EndWhile

    AddEmptyOption()

    _reset = AddTextOption("Reset all settings", "Defaults")

EndEvent

Event OnOptionSelect(Int option)

    If option == _dim || option == _standard || option == _bright

        Float level = 1.0

        If option == _dim

            level = 0.55

        ElseIf option == _bright

            level = 1.5

        EndIf

        Value(0).SetValue(level)

        ForcePageReset()

        Return

    EndIf

    If option == _reset

        Int j = 0

        While j < _defaults.Length

            Value(j).SetValue(_defaults[j])

            j += 1

        EndWhile

        ForcePageReset()

        Return

    EndIf

    Int i = FindOption(option)

    If i >= 0 && _toggles[i]

        Bool enabled = Value(i).GetValue() < 0.5

        If enabled

            Value(i).SetValue(1.0)

        Else

            Value(i).SetValue(0.0)

        EndIf

        SetToggleOptionValue(option, enabled)

    EndIf

EndEvent

Event OnOptionSliderOpen(Int option)

    Int i = FindOption(option)

    If i >= 0

        SetSliderDialogStartValue(Value(i).GetValue())

        SetSliderDialogDefaultValue(_defaults[i])

        SetSliderDialogRange(_minimum[i], _maximum[i])

        SetSliderDialogInterval(_steps[i])

    EndIf

EndEvent

Event OnOptionSliderAccept(Int option, Float amount)

    Int i = FindOption(option)

    If i >= 0

        Value(i).SetValue(amount)

        SetSliderOptionValue(option, amount, "{1}")

    EndIf

EndEvent

Event OnOptionDefault(Int option)

    Int i = FindOption(option)

    If i >= 0

        Value(i).SetValue(_defaults[i])

        If _forms[i] == 2372

            SetKeyMapOptionValue(option, _defaults[i] as Int)

        ElseIf _toggles[i]

            SetToggleOptionValue(option, _defaults[i] > 0.5)

        Else

            SetSliderOptionValue(option, _defaults[i], "{1}")

        EndIf

    EndIf

EndEvent

Event OnOptionHighlight(Int option)

    If option == _reset

        SetInfoText("Restores all settings to defaults, including G for transformation and extra ground fire damage OFF.")

        Return

    EndIf

    If option == _dim || option == _standard || option == _bright

        SetInfoText("Changes only Magic effect brightness: Dim 0.55, Standard 1.0, Bright 1.5. Other visual settings are unchanged.")

        Return

    EndIf

    Int i = FindOption(option)

    If i >= 0

        SetInfoText(_descriptions[i])

    Else

        SetInfoText("Settings are saved with this character.")

    EndIf

EndEvent

Event OnOptionKeyMapChange(Int option, Int keyCode, String conflictControl, String conflictName)

    Int i = FindOption(option)

    If i < 0 || _forms[i] != 2372

        Return

    EndIf

    If keyCode > 255

        ShowMessage("Choose a keyboard key.", False)

        Return

    EndIf

    If conflictControl != "" || conflictName != ""

        If !ShowMessage("This key is already assigned. Use it anyway?")

            Return

        EndIf

    EndIf

    If keyCode < 0

        keyCode = 0

    EndIf

    Value(i).SetValue(keyCode)

    SetKeyMapOptionValue(option, keyCode)

EndEvent
