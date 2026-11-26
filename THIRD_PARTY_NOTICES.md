# Componentes de terceros

EFRT incluye o enlaza los siguientes componentes. Cada uno se rige por su propia
licencia, no por el `EULA.txt`.

| Componente | Titular | Licencia | Dónde se usa |
|---|---|---|---|
| [JUCE](https://juce.com) 7.0.12 | Raw Material Software Limited | Doble licencia: AGPLv3 o licencia comercial de JUCE | Framework del plugin (se descarga al compilar con CMake) |
| Baron Neue / Baron Neue Bold | Frank Hemmekam | Gratis solo para uso personal; uso comercial requiere contactar al diseñador | Tipografía de la interfaz (`Assets/Fonts/`) |
| VST® | Steinberg Media Technologies GmbH | Marca registrada; el SDK VST3 viene incluido en JUCE | Formato de plugin VST3 |

> **Pendiente de documentar:**
> - JUCE: confirmar y anotar aquí el tier de licencia de JUCE que se usa
>   (en JUCE 7 el tier gratuito exige el cartel "Made with JUCE").
> - Baron Neue: se pidió permiso por mail al diseñador. Anotar aquí la fecha y
>   los términos de su respuesta, y conservar el mail.

Los avisos obligatorios de cada licencia deben conservarse en las distribuciones.
