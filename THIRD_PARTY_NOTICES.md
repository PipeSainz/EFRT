# Componentes de terceros

EFRT incluye o enlaza los siguientes componentes. Cada uno se rige por su propia
licencia, no por el `EULA.txt`.

| Componente | Titular | Licencia | Dónde se usa |
|---|---|---|---|
| [JUCE](https://juce.com) 7.0.12 | Raw Material Software Limited | Doble licencia: AGPLv3 o licencia comercial de JUCE | Framework del plugin (se descarga al compilar con CMake) |
| Josefin Sans (SemiBold, Bold) | The Josefin Sans Project Authors (Thomas Jockin) | SIL Open Font License 1.1 (copia en `Assets/Fonts/OFL-JosefinSans.txt`) | Tipografía de la interfaz (`Assets/Fonts/`) |
| VST® | Steinberg Media Technologies GmbH | Marca registrada; el SDK VST3 viene incluido en JUCE | Formato de plugin VST3 |

> **Pendiente de documentar:**
> - JUCE: confirmar y anotar aquí el tier de licencia de JUCE que se usa
>   (en JUCE 7 el tier gratuito exige el cartel "Made with JUCE").
> - Imágenes del fondo (madera, helechos, mármol): anotar aquí su origen y licencia.

Los avisos obligatorios de cada licencia deben conservarse en las distribuciones.
