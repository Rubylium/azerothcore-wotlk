# Pestiféré client art

`localTools/buildPestifereClientAssets.ps1` compiles the PNG sources into WotLK-compatible TGA files.

## Live spell icons

| Icon | Spell ids |
|---|---|
| `Pestifere_FrappePutride` | 90200 |
| `Pestifere_Pourriture` | 90205 |
| `Pestifere_Contagion` | 90201 |
| `Pestifere_Detonation` | 90202 |
| `Pestifere_OdeurCharogne` | 90203 |
| `Pestifere_CrachatBilieux` | 90204 |
| `Pestifere_CarapaceNecrosee` | 90210, 90211, 90220 |
| `Pestifere_ChairPutride` | 90212, 90213, 90221 |
| `Pestifere_PesteVirulente` | 90214, 90215, 90222 |

## Charnier talent icons

The talent implementation described in `.agents/plans/pestifere/pestifere.DESIGN.md` is not yet present in
`Talent.dbc` or the server scripts. Its art is ready under stable names:

- Unlock talents `Chair putride` and `Peste virulente` reuse their matching live spell icon.
- `PestifereTalent_PeauCoriace`
- `PestifereTalent_RageFielleuse`
- `PestifereTalent_InoculationRapide`
- `PestifereTalent_Miasme`
- `PestifereTalent_MainsPutrides`
- `PestifereTalent_Fossoyeur`
- `PestifereTalent_SymbioseMorbide`
- `PestifereTalent_MetabolismeNecrotique`
- `PestifereTalent_DetonationChaine`
- `PestifereTalent_CharnierAmbulant`
- `PestifereTalent_CrouteNecrosee`
- `PestifereTalent_MenaceContagieuse`
- `PestifereTalent_PorteurEndurci`
- `PestifereTalent_PurgeCathartique`
- `PestifereTalent_ResiliencePorteur`
- `PestifereTalent_Sepulcre`

The talent background base name is `PestifereCharnier`. The compiler emits the four exact WotLK quadrants
under `Interface\TalentFrame`.

## Sangsue healer art

The healer specialization uses 36 dedicated `PestifereHealer_*` icons for its active abilities, proc auras,
and talent nodes. `buildPestifereClientAssets.ps1` derives the required icon list from
`localTools/patchSinisterStrike.ps1` and fails the build if a source or compiled icon is missing.

The talent tab icon is `PestifereHealer_Sangsue`. Its background base name is `PestifereSangsue`; the
compiler emits its four WotLK talent-frame quadrants beside the Charnier tiles.
