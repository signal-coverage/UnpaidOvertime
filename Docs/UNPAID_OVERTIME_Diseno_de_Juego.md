# UNPAID: OVERTIME — Documento de diseño de juego

**Versión:** 0.4 (borrador para revisión) · **Motor:** Unreal Engine 5 (C++) · **Plataforma objetivo:** PC / Steam
**Base:** `diseno_juego_oficina_steam.md` + las ideas y cambios que definiste para este proyecto.

> **Qué cambió respecto a la v0.3:**
> - Las **4 a 8 tareas son en total** (no por jugador) y son **cooperativas**: cada tarea se divide en **sub-objetivos** que reparten varios jugadores (por ejemplo, "Mantenimiento de impresoras" = 3 cartuchos para 3 impresoras).
> - Las **estrellas** se calculan por **contribución individual** (sub-objetivos completados por cada uno), no por tareas propias.
> - Se acepta que **sin director de ritmo** puede no salir nadie en un día: la dificultad sube al día siguiente.
> - Se acepta la **duración estimada** (~40 a 45 min con 4 jugadores, día "Normal").
>
> **Qué cambió respecto a la v0.2:**
> - Se puede jugar **desde 1 jugador**: las **IAs completan** hasta un mínimo de **4 en total**, y **suben de dificultad día a día**.
> - El **nombre de la empresa es obligatorio**, sin valor por defecto.
> - Los **días son infinitos** (modo récord, como los niveles de "REPPO"): ya no hay semana fija; los niveles solo se hacen **más difíciles**.
> - Las **tareas** son un **rango de 4 a 8 definido por el jugador antes de la partida** y **no aumentan** con los días.
> - **Un solo ganador**, bueno o malo (el último en pie).
> - **Quién se va** lo decide una **votación** entre los de 1 estrella (se elimina el director de ritmo).
> - El asesino **puede ser despedido, pero nunca por despido directo**: solo por votación.
> - **Muerte por el asesino, final del juego y otras reglas son configurables** antes de empezar.
> - Los **despedidos y muertos** siguen mirando en **tercera persona**, sin micrófono con los que juegan.

---

## 0. Resumen (leer esto primero)

**UNPAID: OVERTIME** es un **juego en primera persona de comedia negra y terror de oficina, para 1 a 8 jugadores**, con **tareas cooperativas y una traición oculta**. Antes de empezar, el anfitrión **escribe el nombre de la empresa (obligatorio)** y configura la partida. Si hay menos de 4 jugadores, **IAs completan** el equipo hasta 4 como mínimo.

El juego es una **sucesión de días, uno por nivel, sin un final fijo**. Cada día, el equipo reparte **tareas** (la cantidad la elige el jugador en la configuración, de 4 a 8 por día) y al cierre el **jefe** (un sistema) da de **0 a 3 estrellas**. Los jugadores con **0 estrellas son despedidos**, y entre los de **1 estrella el grupo vota quién se va**. Con **3 estrellas** se puede intentar una **promoción**, que funciona como **escudo** contra un despido directo.

**Uno de los empleados es el asesino/saboteador.** Sabotea las tareas de los demás para bajarles las estrellas, miente con una **calificación falsa** y deja rastros. **También puede ser despedido, pero nunca por despido directo: solo si lo condenan por votación.**

**Gana un solo jugador: el último que queda en pie**, sea bueno o malo. Quien es despedido o muere **sigue mirando** la partida con una **cámara en tercera persona**, pero **ya no comparte micrófono** con quienes siguen jugando.

**Cada día es más difícil, pero no más largo:** las tareas son las mismas en cantidad y se vuelven **más complejas, más sombrías y más peligrosas**, y las **IAs mejoran**. En modo **solitario o "racha"**, el objetivo es **batir tu propio récord** de días sobrevividos.

**Lo que lo hace distinto**
1. **Las tareas son la investigación:** cada tarea deja un **rastro** (impresora, tarjeta de fichaje, cámaras, correo), y las **estrellas** muestran quién rinde y quién no.
2. **Overtime como mecánica:** si el turno se pasa de hora, empieza el **tiempo extra no pagado**: la oficina se oscurece y el asesino gana ventajas.
3. **Quién se va lo deciden los jugadores**, no el sistema: votar tiene peso social y político.
4. **Se puede jugar solo** contra IAs que mejoran, lo que da un modo **récord rejugable**.

**Alcance recomendado:** empezar por **1 jugador + 3 IAs en un solo día** (sin red). Las IAs son la pieza más importante del plan, porque permiten probar todo en solitario y son el modo principal del juego.

---

## 0.1 Cómo interpreté tus respuestas, y lo que sigue abierto

### Respuestas que apliqué tal cual

| Tema | Tu decisión | Dónde está |
|---|---|---|
| Equipo | 1 jugador + IAs hasta 4 como mínimo; las IAs suben de dificultad. | Secciones 2.1 y 9.2 |
| Nombre de la empresa | Obligatorio, sin valor por defecto. | Sección 2.2 |
| Días | Infinitos; modo récord. | Sección 2.3 |
| Tareas | 4 a 8, elegidas antes de la partida; no aumentan, solo se vuelven más difíciles. | Sección 2.5 |
| ¿El malo mata? | Opcional y configurable. | Sección 6.4 |
| Promoción | Escudo, el jugador se queda. | Sección 4.4 |
| Quién se va | Votación entre los de 1 estrella. | Sección 4.3 |
| Final | Configurable antes de la partida; un solo ganador. | Sección 7 |
| Asesino y despido | Puede ser despedido, solo por votación. | Sección 6.1 |
| Espectadores | Tercera persona, sin micrófono con los vivos. | Sección 7.4 |

### Cosas que interpreté y necesito que confirmes

| # | Punto | Qué entendí / qué elegí por defecto | Por qué lo marco |
|---|---|---|---|
| 1 | **"Días infinitos" y "gana un solo jugador"** | Una **partida** termina cuando **queda un solo jugador** (el ganador). **Dentro de la partida no hay un número fijo de días**. En **solitario** (o modo **"racha"**), si ganas, empieza **una nueva "sucursal" con IAs más difíciles** y el récord de días sigue sumando hasta que pierdas. | Estas dos ideas tiran en direcciones opuestas: si solo gana uno, hay un final. Resolví con "racha". |
| 2 | **"Tareas 4 a 8 divididas entre los jugadores"** | **Resuelto en v0.4:** son **4 a 8 tareas en total**, cooperativas, divididas en sub-objetivos (sección 2.5). | Confirmado por el usuario. |
| 3 | **Apelación de 1 estrella** | Se reemplaza por la **votación** entre los de 1 estrella. Conservé la **pregunta del jefe** como **defensa opcional** antes de votar (si la aciertas, ese día no eres candidato). | Habías definido antes dos opciones de apelación; la votación ya cumple un rol parecido. |
| 4 | **Despido directo (0 estrellas)** | Sigue existiendo para **todos menos el asesino**. El asesino con 0 estrellas reales **no** es despedido directamente. | Es coherente con tu regla de que al malo solo lo saca la votación. |
| 5 | **Cómo se condena al asesino** | **Votación en la reunión del día** (cualquier jugador vivo puede pedir una **acusación formal** con **3 pruebas**), además de la votación de recorte. | Si el asesino muestra 3 estrellas, no sería candidato en la votación de 1 estrella. Hace falta otra vía para condenarlo. |
| 6 | **Duración mínima de una hora** | Al no haber días fijos, la duración **depende de cuántos salen por día**. Hay un **selector de duración del día** y estimaciones en la sección 3. | Es **emergente**: no se puede garantizar, hay que medirla. |
| 7 | **IAs como jugadores** | Las IAs **hacen tareas, votan y pueden ser el asesino**, con reglas simples (sin aprendizaje). | Es la parte más cara del proyecto. Ver riesgos. |

---

## 1. Premisa, tono y fantasía

> *"Termina tus tareas, saca tus estrellas y sé el último en pie en una empresa donde cada fin de turno alguien se va… y no siempre por las buenas."*

### Ambientación

La empresa tiene **el nombre que escribe quien crea la sala** (obligatorio). Aparece en carteles, correos, tarjetas de identificación, memos y anuncios por altavoz, con **validación** de longitud y de palabras prohibidas.

### Tono

- **Comedia negra:** reuniones absurdas, jerga corporativa ("sinergia", "desvinculación", "cultura de resultados").
- **Misterio y traición:** hay un saboteador entre ustedes.
- **Terror suave "family friendly":** luces que parpadean, **sangre sutil**, **partes de cuerpos o cuerpos completos** como ambientación y **cartas de despido** colgadas por la oficina que advierten de las consecuencias de no seguir las reglas. Humor macabro estilizado, sin realismo gráfico.

> **Nota:** el contenido violento debe mantenerse estilizado. Steam pide declarar el contenido (violencia, sangre, temas) al publicar; revisar sus políticas y clasificaciones por edad antes del lanzamiento.

### Referencias para estudiar (no copiar)

*Among Us* (deducción social con tareas), *Lockdown Protocol* (cooperativo con un infiltrado), *Lethal Company* y *Project Winter* (cooperación y tensión en primera persona), *The Stanley Parable* (la oficina y la narración) y el juego de niveles progresivos que mencionaste, "REPPO", como referencia del modo récord.

---

## 2. Estructura de la partida

### 2.1 Jugadores e IAs

- Se puede jugar con **1 a 8 jugadores humanos**.
- Si hay **menos de 4 en total**, **IAs completan el equipo hasta 4** (por ejemplo, 1 humano + 3 IAs).
- El anfitrión puede **agregar más IAs** hasta un total de **8**.
- Cada IA es un **empleado completo**: hace tareas, recibe estrellas, vota, puede ser despedida y puede ser el **asesino**.
- Las IAs **suben de dificultad** con los días (sección 9.2).

### 2.2 Configuración previa a la partida

Todo esto se elige **antes de empezar** y no cambia durante la partida:

| Opción | Valores | Por defecto |
|---|---|---|
| **Nombre de la empresa** | Texto de hasta 24 caracteres. **Obligatorio.** | **Ninguno**: el botón "Crear" no se activa hasta escribirlo. |
| **Jugadores humanos** | 1 a 8 | 1 |
| **IAs** | Las necesarias hasta 4 en total, hasta 8 | Las que completen 4 |
| **Tareas por día (en total)** | **4 a 8**, repartidas entre todos los empleados | 6 |
| **El asesino puede matar** | Sí / No | No |
| **Final** | Duelo / Último en pie / Votación final | Duelo |
| **Duración del día** | Corta / Normal / Larga | Normal |
| **Semilla** | Aleatoria o compartida | Aleatoria |
| **Voz de los espectadores** | Canal aparte / Solo texto / Silencio | Canal aparte |
| **Retardo del espectador** | Sí / No (reduce las filtraciones) | No |
| **Dificultad base de las IAs** | Fácil / Media / Difícil | Media |

### 2.3 Días infinitos y el modo "racha"

- **No hay un número fijo de días.** Los días siguen mientras haya más de un jugador en pie.
- Cada día es un **nivel nuevo**, generado con la semilla, **más difícil** que el anterior.
- Los días se **nombran** en ciclos de semana (Lunes, Martes…, Viernes y luego un **fin de semana de "horas extra"**), con un contador acumulado: *"Día 12 — Martes (Semana 3)"*.

**Ganar la partida:** queda **un solo jugador** (humano o IA). En **multijugador**, ahí termina la partida.

**Modo "racha" (para 1 jugador, o en grupo si lo eligen):**
- Si **el jugador humano gana**, **empieza una nueva "sucursal"** con un grupo nuevo de IAs más difíciles y **el contador de días sigue**.
- Se pierde cuando **te despiden, te matan o te condenan**.
- El **récord** es la cantidad de **días sobrevividos** (con tabla de puntajes de Steam).

### 2.4 Las fases de cada día

```
Turno de trabajo (tareas) → [OVERTIME si se pasa de hora] → Evaluación (estrellas)
   → Despidos directos → Votación de recorte (los de 1 estrella) → Reunión (discusión y acusación formal) → Siguiente día
```

| Fase | Qué pasa |
|---|---|
| **Turno** | Todos hacen tareas. El asesino sabotea. Hay reloj y, si se pasa, **OVERTIME**. |
| **Evaluación** | El jefe da de **0 a 3 estrellas** a cada empleado. |
| **Despidos directos** | Los de **0 estrellas** son despedidos (salvo el asesino y quienes tengan escudo). |
| **Votación de recorte** | Entre los de **1 estrella**, el grupo vota **quién se va** (sección 4.3). |
| **Reunión** | Se discute, se comparan rastros y se puede pedir una **acusación formal** (sección 6.3). |

### 2.5 Tareas: la cantidad no sube, la dificultad sí

- El jugador elige entre **4 y 8 tareas en total por día** y **esa cantidad se mantiene durante toda la partida**.
- Las tareas son **cooperativas**: cada una se divide en **sub-objetivos** que cualquier empleado vivo puede tomar. Ejemplos: *"Mantenimiento de impresoras"* (hallar el cartucho de cada una de las 3 impresoras) o *"Reponer café"* (llamar para rellenar las 3 cafeteras).
- Cada sub-objetivo completado queda **registrado a nombre de quien lo hizo** (también como rastro, sección 6.3).
- **Lo que sube** con los días es la **dificultad de cada tarea** (sección 9.1): más sub-objetivos, más pasos, más tiempo requerido, más cooperación, más sabotajes, más interferencia del terror.
- Cuando un empleado sale de la partida, **sus sub-objetivos pendientes pasan a los vivos**. El generador mantiene siempre **todas las tareas alcanzables** con los jugadores que quedan.

---

## 3. Duración de una partida

Como no hay días fijos, la duración **no se puede prometer**: depende de **cuántos salen cada día**. Una partida de N jugadores necesita **al menos N − 1 salidas** para tener un ganador.

### Estimaciones iniciales (sin probar)

| Duración del día | Tiempo por día |
|---|---|
| Corta | 8 a 10 min |
| Normal | 12 a 15 min |
| Larga | 18 a 20 min |

| Jugadores | Días necesarios si sale 1 por día | Duración aproximada (día "Normal") |
|---|---|---|
| 4 | ~3 | ~40 a 45 min |
| 6 | ~5 | ~60 a 75 min |
| 8 | ~7 | ~85 a 105 min |

> **Esto no llega a una hora con 4 jugadores en duración "Normal".** Si quieres que siempre dure al menos una hora, las opciones son: usar la duración **"Larga"** por defecto con equipos pequeños, **agregar un prólogo** y un **día final doble**, o que el número de salidas diarias sea **menor a 1** (algunos días sin salidas). **Hay que medir la duración real con pruebas de juego.**

### Por qué puede durar más de lo esperado

- Si todos rinden bien, **nadie queda con 1 o 0 estrellas** y no sale nadie. La partida sigue.
- Por eso la dificultad creciente es clave: tarde o temprano **alguien falla**. Es parte del diseño de "días infinitos".

---

## 4. Calificación, despidos y promoción

### 4.1 Las estrellas

Al final del día, el jefe califica a cada empleado **vivo** de **0 a 3 estrellas** según su **contribución individual**: la **cuota justa** es el total de sub-objetivos del día dividido entre los empleados vivos, y se mide qué porcentaje de esa cuota completó cada uno:

| Estrellas | Condición (valores iniciales, ajustables) |
|---|---|
| ★★★ | **100 % o más** de su cuota. |
| ★★☆ | **50 % a 99 %**. |
| ★☆☆ | **Más de 0 % y menos de 50 %**. |
| ☆☆☆ | **0 sub-objetivos**. |

Los **sabotajes** pueden anular un sub-objetivo, lo que baja la contribución de un compañero **sin que sea su culpa**; por eso el sub-objetivo saboteado queda marcado y se puede **rehacer** si hay tiempo.

> **Pendiente de prueba:** que un jugador no pueda "robar" todos los sub-objetivos fáciles. Posible mitigación: sub-objetivos con **peso** distinto según su dificultad.

### 4.2 Qué pasa según las estrellas

| Estrellas | Consecuencia |
|---|---|
| ☆☆☆ (0) | **Despido directo** (el asesino queda exceptuado). Un **escudo** lo evita una vez. |
| ★☆☆ (1) | Pasa a ser **candidato** en la **votación de recorte** (4.3). |
| ★★☆ (2) | Queda **libre**. |
| ★★★ (3) | Queda libre y puede **intentar la promoción** (4.4). |

### 4.3 Votación de recorte: quién se va

- Los **candidatos** son **solo los jugadores con 1 estrella visible** (*"seleccionables solo los de 1 estrella"*).
- **Todos los jugadores vivos votan** a uno de los candidatos; **el más votado se va**.
- **Empate:** cada candidato empatado responde una **pregunta del jefe**; se va quien **peor responda** (o el primero en fallar).
- **Defensa opcional:** antes de votar, un candidato puede **responder una pregunta del jefe**. Si acierta, **ese día no es candidato**; si falla, **sigue como candidato**.
- **Sin candidatos** (nadie sacó 1 estrella): **no hay recorte** ese día.
- **Un solo recorte por día**, para que la partida no se vacíe de golpe.

> **El asesino** muestra una **calificación falsa** (sección 6.2). Si muestra **1 estrella**, **puede ser votado**. Eso le da una elección táctica: parecer débil (y confiable) o arriesgarse a cero votos mostrando 3.

### 4.4 Promoción (escudo)

- Con **3 estrellas**, el jugador responde una **pregunta del jefe** (más difícil que las de defensa).
- **Si acierta**: gana la **promoción**, que es un **escudo**: **anula un despido directo** (0 estrellas). El jugador se queda.
- **Si falla**: no pierde nada.
- **Un escudo a la vez** (a decidir si se acumulan).
- **No sirve contra la votación de recorte ni contra una condena** (por ahora; es una decisión abierta).

### 4.5 Quién se va si el sistema no decide

Como el recorte lo **vota el grupo**, **no hay director de ritmo**: el ritmo de salidas depende de **cuánta gente falla** y de **cómo vote la sala**. Esto es más **social**, pero se vuelve **más difícil de balancear** (ver riesgos).

---

## 5. El jefe (sistema)

El jefe **no es un jugador ni una IA conversacional**. Es un **sistema** que:

1. **Evalúa** (asigna estrellas).
2. **Hace preguntas** sobre la oficina para las defensas, los empates y las promociones.
3. **Sirve de referencia** para juzgar si alguien merece promoción o despido.

> **Alcance:** no se agrega inferencia ni diálogo abierto por ahora.

### Las preguntas del jefe

Se **generan a partir del estado real del nivel**, para que sean justas:

| Dificultad | Ejemplos |
|---|---|
| **Defensa / desempate** | *"¿De qué color es la máquina de café?"*, *"¿Cuántas plantas hay en la sala de reuniones?"* |
| **Promoción** | *"¿Qué decía el cartel del pasillo norte?"*, *"¿Quién imprimió el informe de las 10:40?"* |

**Reglas:** opciones múltiples (3 a 4), tiempo límite, y **la pregunta nunca depende de algo que el jugador no podía haber visto** (el generador lo valida). Sube la **dificultad** con los días.

---

## 6. El asesino y la investigación

### 6.1 Reglas del rol

- **Uno solo por partida**, asignado al azar y en secreto (humano **o IA**).
- Tiene **tareas falsas**: parece que las hace, pero **no cuentan** y **no dejan los mismos rastros**.
- Muestra una **calificación falsa** visible para los demás.
- **No puede recibir despido directo** (0 estrellas). **Sí puede ser despedido, pero solo si lo condenan por votación** (recorte o acusación formal).
- Su meta: **ser el último en pie**.

### 6.2 La calificación falsa

Al cierre del día, **los demás ven las estrellas que "sacó" el asesino**. Él **elige** entre las que su ficha permite.

| Límite | Para qué sirve |
|---|---|
| Solo valores **plausibles** según los rastros que dejó. | Que la mentira **pueda detectarse**. |
| **Pocos "retoques"** por partida. | No es una herramienta ilimitada. |
| Si muestra **1 estrella**, **es candidato** al recorte. | Una **decisión táctica**: parecer débil o arriesgarse. |
| Su rastro real **no coincide** con lo mostrado. | La **inconsistencia** es la pista clave. |

### 6.3 Sabotaje, rastros y acusación formal

**Sabotajes** (para bajar las estrellas de otros):

| Sabotaje | Efecto | Rastro que deja |
|---|---|---|
| Atascar la impresora | La tarea no avanza. | Registro de error con hora. |
| Cambiar un archivo | La tarea se considera incorrecta. | Historial de la computadora. |
| Bloquear una estación | Hay que esperar. | Tarjeta de fichaje y puerta. |
| Cortar una cámara o luz | Zona ciega momentánea. | Registro de la cámara. |
| Mover objetos | Cambia lo que ve el jugador (afecta las preguntas del jefe). | Marca en el inventario. |

**Los rastros** están en terminales consultables y sirven para **verificar coartadas**.

**Acusación formal** (cómo se condena al asesino aunque muestre 3 estrellas):
- Cualquier jugador vivo puede pedirla en la **reunión del día** (una por día).
- Exige **3 pruebas** adjuntas de los rastros.
- **Todos votan.** Si hay **mayoría**, el acusado **se va**.
- Si el acusado era el **asesino**, el acusador gana una **promoción** (escudo).
- Si era **inocente**, el acusador **pierde su escudo** (si lo tenía) y el grupo pierde un jugador.

### 6.4 Opción: el asesino puede matar (configurable)

Si el anfitrión lo activa, el asesino puede **"desvincular" directamente** a un jugador:
- Con **enfriamiento**, **máximo 1 por día** y con **pocos testigos cerca**.
- Aparece un **cuerpo** con una **carta de despido** (gore suave) y se activa una **reunión de emergencia**.
- **Acelera la reducción del grupo**, lo que cambia la duración y el equilibrio.

### 6.5 Dificultad del asesino

Tu idea de **facilitarle el inicio al malo y dificultarle a medida que pasan los días**, en cada palanca:

| Palanca | Primeros días | Días avanzados |
|---|---|---|
| **Cobertura** (jugadores vivos) | Alta: muchos sospechosos. | Baja: pocos sospechosos. |
| **Calificación falsa** | Mucho margen. | Poco margen: debe coincidir con rastros acumulados. |
| **Enfriamiento y "calor"** | Sabotajes baratos y con poco rastro. | Sabotajes **más visibles** y con más rastros. |

El **OVERTIME** y el **duelo final** le dan herramientas extra para que el final sea tenso.

---

## 7. Cierre de la partida y espectadores

### 7.1 Un solo ganador

Gana **un jugador: el último que queda en pie**, sea **bueno o malo**.

| Quién gana | Qué obtiene |
|---|---|
| **Un empleado** | La **promoción final**; y, si fue quien **condenó** al asesino, un reconocimiento extra. |
| **El asesino** | **"El asesino asciende"**. |

### 7.2 Final configurable

| Opción | Cómo funciona |
|---|---|
| **Duelo** *(por defecto)* | Cuando **quedan dos jugadores**, empieza un día final: el **superviviente "bueno"** debe completar todas sus tareas mientras el asesino lo sabotea y, **solo en el duelo**, puede intentar **eliminarlo**. Si lo logra, gana el bueno; si no, el asesino. |
| **Último en pie** | Los días siguen con las reglas normales hasta que **queda uno**. Con dos jugadores, la votación de recorte decide. |
| **Votación final** | Con **3 o más** vivos, hay una **Junta final** donde todos votan. |

> **Dos jugadores sin asesino:** si el asesino ya salió, quedan solo empleados; sigue el modo "último en pie" hasta quedar uno, y no hay duelo.

### 7.3 Qué pasa en solitario

- Tu IA **compite como cualquier otro empleado**.
- **Si pierdes** (te despiden o te matan), la partida **termina para ti** y se **registra tu récord de días**. Puedes **seguir mirando** o **saltar** al resumen.
- **Si ganas**, empieza una **nueva sucursal** (modo "racha") con IAs más difíciles.

### 7.4 Espectadores (despedidos y muertos)

Quien sale de la partida:
- Sigue **viendo la partida** con una **cámara en tercera persona** que puede **seguir a un jugador** o moverse libre.
- **Ya no comparte micrófono** con los que siguen jugando.
- Puede hablar en un **canal aparte** con los demás espectadores (según la configuración).

**Riesgos y mitigaciones**
- **Filtraciones:** un espectador podría **ver quién es el asesino** y avisar por otro medio (Discord, por ejemplo). Mitigaciones: **retardo del espectador** (opción de la configuración), **no mostrar los roles** en la vista de espectador hasta el final, o limitar la cámara a lo que ven los vivos.
- **Aburrimiento:** los espectadores pueden **votar** en la Junta final (si se activa) y tener un **minijuego pasivo** (apostar quién gana).

---

## 8. OVERTIME: el reloj y el tiempo extra

- Cada día tiene un **reloj de turno**. Si se acaba y faltan tareas, comienza el **OVERTIME**.
- El día **no termina** hasta completar el trabajo, pero hay un **tope** (por ejemplo, +50 % del turno): al llegar a él el día **se cierra a la fuerza** y las tareas pendientes cuentan como **no hechas** (así nadie bloquea la partida).

| En el OVERTIME… | Efecto |
|---|---|
| La oficina se **oscurece** | Menos luz, más sombras. |
| Aumentan las **anomalías** | Más eventos extraños. |
| El asesino **gana ventajas** | Menor enfriamiento de sabotaje. |
| **Penalización** | Pérdida de puntaje: "horas extra no pagadas". |

---

## 9. Dificultad creciente, niveles aleatorios e IAs

### 9.1 La dificultad de las tareas (sube; la cantidad no)

Cada día se aplica un **nivel de dificultad** `D(día)` que sube, pero **la cantidad de tareas se mantiene** (la que eligió el jugador):

| Qué sube | Ejemplo |
|---|---|
| **Pasos por tarea** | Imprimir y engrapar → imprimir, revisar, firmar y archivar. |
| **Tiempo requerido** | Tareas más lentas. |
| **Cooperación** | Más tareas que exigen **dos personas**. |
| **Presión** | Más tareas con **tiempo límite** y **órdenes que se contradicen**. |
| **Interferencia** | Más **sabotajes**, anomalías y cortes. |
| **Tono** | Más **terror**, más gore suave y más historias inquietantes. |

### 9.2 La dificultad de las IAs

Las IAs mejoran con `D(día)`, **sin aprendizaje automático**: son **reglas con parámetros** que se ajustan:

| Parámetro | Efecto al subir |
|---|---|
| **Velocidad** | Completan tareas más rápido (o más lento, si la IA es el "jefe" del reto). |
| **Precisión** | Cometen menos errores y rinden más estrellas. |
| **Sospecha** | Detectan mejor las **inconsistencias** de los rastros al **votar**. |
| **Astucia (si es asesina)** | Sabotea con menos rastros y finge mejor la calificación falsa. |
| **Reacción** | Responde más rápido a una **acusación**. |

> **Las IAs no deben hacer trampa:** **no ven información oculta** que un humano no vería (por ejemplo, quién es el asesino) salvo que sea ellas mismas. Su "sospecha" se calcula **solo con rastros visibles**.

### 9.3 Niveles aleatorios

Cada día se genera con una **semilla** (compartible):

| Qué varía | Cómo |
|---|---|
| **Distribución** | Salas **modulares** conectadas de formas distintas. |
| **Tareas** | De bolsas por día, según el **presupuesto de dificultad** y la cantidad elegida. |
| **Eventos y modificadores** | *"Hoy hay auditoría"*, *"Falla la luz del ala norte"*. |
| **Pistas y rastros** | Con reglas para que **siempre se pueda resolver el caso**. |
| **Capítulo de historia** | Uno por "semana" de días (cada 5 días cambia el arco). |

**Reglas de justicia:** toda tarea debe ser alcanzable; el generador valida que **existan pruebas suficientes** y que **las preguntas del jefe sean respondibles**; se prueba con **cientos de semillas**.

---

## 10. Historia y terror

### Estructura narrativa con días infinitos

Como no hay un viernes fijo, la historia se cuenta en **capítulos de una "semana" de 5 días** (y luego un fin de semana). Cada capítulo:
- **Día 1 del capítulo:** humor y presentación.
- **Días 2 y 3:** primeras anomalías y **cartas de despido**.
- **Día 4:** **giro** de la historia, cambia la oficina.
- **Día 5 / fin de semana:** **revelación** y tensión. Luego empieza otro capítulo, **más oscuro**.

A más capítulos, **más corrupción de la oficina**: el tono se oscurece a la par de la dificultad.

### Gore "family friendly"

- **Sangre sutil** (manchas y salpicaduras estilizadas) y **partes de cuerpos o cuerpos completos** como ambientación: un zapato bajo la puerta, una mano en la trituradora, siluetas.
- **Cartas de despido** colgadas por la oficina: *"El incumplimiento de las normas tendrá consecuencias"*, señalando esas partes.
- **Estilizado, no realista**: colores planos, sin detalle anatómico.

### Arcos (uno por capítulo, al azar)

1. **La cláusula 13.** El edificio **cobra el tiempo** de quienes se quedan.
2. **El becario eterno.** Un pasante que nunca se fue sigue "trabajando".
3. **La fusión.** La empresa será absorbida por otra peor.

Se cuentan con correos, memos, carteles con **el nombre de la empresa del anfitrión**, anuncios por altavoz y cambios en el escenario.

---

## 11. Diseño técnico en Unreal Engine (C++)

**Principio:** el **código define los sistemas** y los **datos definen el contenido**.

### Clases principales

| Clase | Función |
|---|---|
| `UUOGameInstance` | Sesiones online, ajustes, semilla, **récords**. |
| `FMatchConfig` | **Configuración previa**: nombre de la empresa, tareas por jugador, final, asesino que mata, duración, IAs. |
| `AUOGameMode` | Reglas del día, reloj, OVERTIME, fases, **final de partida y racha**. |
| `AUOGameState` | Estado replicado: día, fase, jugadores vivos, **nombre de la empresa**. |
| `AUOPlayerState` | Rol (secreto), **vivo / espectador**, tareas, **estrellas**, **escudo**. |
| `AUOCharacter` | Primera persona, interacción y carga. |
| `AUOSpectatorPawn` | **Cámara en tercera persona** para despedidos y muertos. |
| `UInteractionComponent` | Apunta e interactúa. |
| `ATaskStation` + `UTaskDefinition` | Estación y definición de tarea (**Data Asset**): pasos, duración, **rastros**, **sabotajes posibles**, **nivel de dificultad**. |
| `UPerformanceSystem` | Cuenta tareas, calcula **estrellas**, aplica **despidos directos** y el **recorte**. |
| `UVoteSystem` | Votaciones: **recorte, empate y acusación formal**. |
| `UBossSystem` | Genera y corrige las **preguntas del jefe**. |
| `UEvidenceSubsystem` | Rastros y terminales. |
| `URoleSystem` | Asignación del rol y validación de las **acciones del asesino** en el servidor. |
| `UDifficultySubsystem` | `D(día)` para tareas, IAs y terror. |
| `UDirectorSubsystem` | Eventos, anomalías y OVERTIME. |
| `ALevelGenerator` | Oficina modular con **semilla**. |
| `AUOAIController` | **IA de empleados** (State Tree): tareas, votos, sospecha, rol de asesino. |
| `UVoiceChannelManager` | **Voz**: canal de los vivos, canal de los espectadores y proximidad. |

### Nombre de la empresa

Obligatorio, validado **en el servidor** (24 caracteres, sin caracteres de control, filtro de palabras) y **replicado** a todos como parte del estado del juego.

### Principios de red

- **Servidor autoritativo:** tareas, **estrellas, despidos, votos, sabotajes y roles** se deciden en el servidor.
- **Información secreta** (quién es el asesino, la calificación real) **no se replica** a los demás clientes. En la **vista de espectador**, los roles **no se muestran** (o con retardo, según la configuración).
- Comienzo con **servidor "listen"** y **Steam Online Subsystem**. Con **1 jugador** el juego **corre sin red**: las IAs funcionan en local.
- **Voz:** proximidad para los vivos; **canal separado** para los espectadores (decisión de proveedor pendiente).

### Datos (para no recompilar)

Tareas (**Data Assets**), eventos, preguntas del jefe, historias, parámetros de IA y salas (**Data Tables**), y textos en **tablas de localización** (ES/EN).

### Pruebas automáticas

Lógica pura con el framework de pruebas de Unreal: **cálculo de estrellas**, reglas de despido, **votación de recorte y desempates**, **condiciones de victoria**, validación de **configuración** y **generador de semillas**.

### Qué se reutiliza de la versión web

| De la versión web | Uso en Unreal |
|---|---|
| Interacción (apuntar + usar) | `UInteractionComponent`. |
| Sentarse, emotes, estados | Animaciones y estados del personaje. |
| Personalización del personaje | Cosméticos del metajuego. |
| Salas, zonas y plantillas de estructura | Diseño del generador de niveles modulares. |
| Pruebas e2e y "servidor autoritativo" | Pruebas automáticas y validación. |

---

## 12. Metajuego y progresión

- **Récords de días sobrevividos** (modo racha), con tabla de puntajes de Steam por dificultad y por tamaño de equipo.
- **Rango de carrera:** Pasante → Asistente → Analista → Coordinador → Gerente → Director.
- **Cosméticos** (ropa, accesorios, decoración del escritorio, tarjetas de identificación).
- **Logros** de Steam: "Despedido injustamente", "Cero horas extra", "El último en pie".
- **Semilla compartida** y retos diarios.
- **Sin ventaja de poder pagada.**

---

## 13. Dirección artística y sonora

3D estilizado de baja complejidad, paleta limitada (beige, gris, fluorescente) con acentos rojo "finiquito" y verde enfermizo. Iluminación expresiva como herramienta de miedo. Sonido: fluorescentes, teclado, impresoras y un altavoz que pasa de amable a inquietante.

---

## 14. Plan de desarrollo por hitos

Esfuerzos orientativos (S/M/L).

| Hito | Contenido | Resultado | Esfuerzo |
|---|---|---|---|
| **M0** | Git, `.gitignore` de Unreal, Git LFS, `CLAUDE.md`, servidor MCP funcionando. | Entorno listo. | S |
| **M1** | Oficina "greybox", movimiento, interacción, **3 tipos de tarea**. | Un jugador hace tareas. | M |
| **M2** | **IAs básicas** (hacen tareas), reloj del día, **estrellas** y evaluación, OVERTIME. | **1 jugador + 3 IAs: un día jugable.** | L |
| **M3** | **Despidos directos, votación de recorte, jefe y preguntas, promoción-escudo.** | Ciclo de evaluación completo en solitario. | L |
| **M4** | **Asesino** (humano o IA): tareas falsas, **sabotajes, calificación falsa, rastros, acusación formal.** | Misterio jugable. | L |
| **M5** | **Fin de partida**: último en pie, duelo, **espectador** en tercera persona, **modo racha** y récords. | **Partida completa en solitario.** | L |
| **M6** | **Dificultad creciente** (tareas e IAs) y **generador de niveles**. | Días infinitos. | L |
| **M7** | **Multijugador 2 a 8** (servidor listen), voz de proximidad y **canal de espectadores**. | Partidas en grupo. | L |
| **M8** | **Vertical slice**: arte, sonido, narrativa y gore suave. | Demostración de calidad. | L |
| **M9** | Steam (sesiones, logros, tabla de récords, demo) y **pruebas con jugadores**. | Demo pública. | M |
| **M10** | Contenido, arcos de historia, balance y pulido. | Versión candidata. | L |

> **Cambio importante en el plan:** las **IAs y el modo solitario van primero** (hitos M2 a M6), y el **multijugador después** (M7). Así el juego ya funciona y se puede probar **sin depender de la red**.

### Qué medir en las pruebas con jugadores

- ¿Entienden las **estrellas** y la **votación de recorte**?
- ¿Las **IAs** parecen jugadores creíbles, o se nota que son bots?
- ¿Las tareas son divertidas **antes** de sumar al asesino?
- ¿Qué tan **larga** resulta cada partida de verdad?
- ¿El modo **racha** engancha para batir récords?
- ¿Se siente **justa** la votación (¿se vota por rencor o por razones?)?

---

## 15. Riesgos y mitigaciones

| Riesgo | Mitigación |
|---|---|
| **IAs creíbles** como jugadores (hacen tareas, votan, mienten, sospechan). Es la pieza más cara. | **Reglas simples** con parámetros (no aprendizaje), pruebas desde el primer hito de IA y comportamientos acotados. |
| **Equilibrio del ritmo** sin director: puede no salir nadie durante mucho tiempo, o salir demasiados. | La **dificultad creciente** fuerza fallos; la configuración de **duración del día**; medir y ajustar. Se puede sumar un **director de ritmo opcional** más adelante. |
| **Duración de la partida** (no se garantiza una hora). | Selector de duración, prólogo y día final doble; **medir** con pruebas. |
| **Votaciones tóxicas** (se vota por rencor, o siempre al mismo). | Votos **anónimos opcionales**, la **defensa con pregunta del jefe** y que solo sean candidatos los de **1 estrella**. |
| **Filtración de espectadores** (avisan quién es el asesino). | Retardo opcional, canal de voz separado y no mostrar los roles. |
| **Sabotaje injusto** (se pierde por algo ajeno). | Los sabotajes dejan **rastros**, existe la **defensa con el jefe** y se ajusta con pruebas. |
| **Un jugador solo "pierde" demasiado rápido.** | Dificultad base **Fácil/Media/Difícil** y IAs que mejoran **gradualmente**. |
| **Modo solitario sin gente** | Funciona **sin red**; el multijugador es opcional. |
| **Nombre de empresa ofensivo** | Validación en servidor y lista de palabras prohibidas. |
| **Contenido gráfico y clasificación** | Estilizado, humor negro, declarar el contenido en Steam. |
| **Multijugador online** | Va **después** del solitario; servidor autoritativo desde el comienzo. |
| **Nombre del juego** | Revisar marcas y disponibilidad antes de comprometerse. |

---

## 16. Decisiones abiertas

| # | Decisión | Mi propuesta por defecto | Alternativas |
|---|---|---|---|
| 1 | ~~"4 a 8 tareas": por jugador o en total~~ | **Resuelto (v0.4):** en total, cooperativas, con sub-objetivos y estrellas por contribución. | — |
| 2 | **Modo racha** | Activo en solitario; opcional en grupo. | Solo en solitario. |
| 3 | **Escudo de promoción** | Solo contra el **despido directo**, de **un uso**. | También contra el **recorte**; acumulable. |
| 4 | **Votos del recorte** | **Públicos**. | **Anónimos** (menos rencor). |
| 5 | **Defensa con pregunta del jefe** | Sí, antes de votar. | No (solo votación). |
| 6 | **Acusación formal** | Una por día, con **3 pruebas**. | Solo en la Junta final. |
| 7 | **Asesino IA** | Sí, puede ser una IA. | Solo humano (si hay humanos suficientes). |
| 8 | **Espectadores** | Canal de voz aparte; sin ver los roles. | Ver los roles; sin voz. |
| 9 | **Duración del día** | Normal (12 a 15 min). | Corta o Larga. |
| 10 | **Dificultad base de las IAs** | Media. | Fácil o Difícil. |
| 11 | **Voz** | Proximidad + reuniones. | Solo texto. |
| 12 | **Perspectiva** | Primera persona. | Tercera persona. |
| 13 | **Idiomas** | Español e inglés. | Solo español al principio. |

---

## 17. Glosario

- **Estrellas:** calificación diaria de 0 a 3 según tareas completadas.
- **Despido directo:** salida inmediata por 0 estrellas (salvo el asesino).
- **Votación de recorte:** el grupo vota quién se va entre los de 1 estrella.
- **Promoción:** premio de 3 estrellas + pregunta correcta; funciona como escudo.
- **Jefe:** sistema que evalúa y pregunta; no es un jugador.
- **Calificación falsa:** estrellas que el asesino muestra a los demás.
- **Sabotaje:** acción del asesino que impide una tarea ajena.
- **Rastro:** registro que dejan tareas y acciones; base de la investigación.
- **Acusación formal:** votación con 3 pruebas para condenar a un jugador.
- **Duelo:** final con dos jugadores.
- **Sucursal:** nueva partida del modo racha, con IAs más difíciles.
- **Espectador:** jugador despedido o muerto que mira en tercera persona.
- **Overtime:** tiempo extra no pagado, cuando se pasa la hora del turno.
- **Semilla:** número que define cómo se genera un nivel.

---

## 18. Próximos pasos

1. **Revisar la sección 0.1** (interpretaciones) y la **16** (decisiones abiertas).
2. **M0:** repositorio Git con LFS, `CLAUDE.md`, plugins de Python y Remote Control activados y el MCP probado.
3. **M1 y M2:** prototipo con **1 jugador y 3 IAs** de un día completo con estrellas y OVERTIME, y **medir cuánto dura de verdad**.
4. Probar el bucle de tareas con IAs **antes** de sumar el asesino.

---

## 19. Evaluación del diseño (mi opinión, para tenerla a mano)

### Lo más fuerte

- **Las estrellas dan una razón concreta para sospechar.** Si alguien baja de estrellas, ¿fue mala suerte, es lento o lo sabotearon? Las tareas, los rastros y las estrellas se refuerzan entre sí.
- **Que decidan los jugadores** (la votación de recorte) le da peso social al juego: se negocia, se acusa, se defiende.
- **La calificación falsa del asesino** crea una decisión táctica real: mostrar 1 estrella para parecer débil (y arriesgarse a un voto) o mostrar 3 (y arriesgarse a que lo delaten los rastros).
- **El modo solitario con IAs y el récord de días** hacen el juego **rejugable aunque no haya gente**, que es el mayor problema de los juegos sociales.
- **OVERTIME** convierte el título en mecánica: el tiempo extra no pagado es peligroso y entra en la narrativa.

### Lo más riesgoso

1. **Las IAs creíbles.** Tienen que hacer tareas, votar, sospechar y mentir de forma convincente, y ahora son el **modo principal**. Mitigación: reglas simples con parámetros, sin aprendizaje, y pruebas desde el primer hito de IA.
2. **El ritmo ya no está garantizado.** Sin director de ritmo, si todos rinden bien, nadie sale. La dificultad creciente fuerza fallos, pero hay que medirlo.
3. **La duración.** Al no haber días fijos, una partida de 4 jugadores con días "Normal" ronda los **40 a 45 minutos**, no una hora. Hay tres palancas en la sección 3.
4. **Con 4 jugadores el asesino es obvio rápido.** Los bots, la calificación falsa y los sabotajes con poco rastro al principio ayudan, pero hay que probarlo.
5. **Un sabotaje puede hacer perder a alguien por algo ajeno.** Por eso los sabotajes dejan rastros y existe la defensa con la pregunta del jefe.
6. **Los espectadores pueden filtrar información** por otro medio (Discord, por ejemplo). Opciones: retardo, no mostrar los roles, canal aparte.
7. **Votaciones tóxicas** (rencor, siempre al mismo). Opciones: votos anónimos, defensa del jefe, solo los de 1 estrella son candidatos.

### Qué cambié de plan por esto

**Las IAs y el modo solitario van primero (hitos M2 a M6); el multijugador después (M7).** Así el juego funciona y se puede probar sin depender de la red.

---

## 20. Historial de versiones

| Versión | Cambios |
|---|---|
| **0.1** | Primera propuesta: semana de 5 niveles (lunes a viernes), 2 a 6 jugadores, 4 a 6 tareas, asesino que mata, Junta del viernes. |
| **0.2** | Equipo de 4 a 8; **días = jugadores**; 4 a 8 tareas por jugador, que **aumentan** con los días; **nombre de la empresa** elegido por el anfitrión; **estrellas, despidos, apelaciones y promoción como escudo**; jefe como sistema; asesino con calificación falsa y sabotaje; **gore suave**; plan de duración. |
| **0.3** | **1 a 8 jugadores con IAs** hasta 4 como mínimo; nombre de la empresa **obligatorio y sin valor por defecto**; **días infinitos** (modo récord y "racha"); tareas **fijas (4 a 8) y solo más difíciles**; **un solo ganador**; **votación de recorte** entre los de 1 estrella (sin director de ritmo); el asesino **puede ser despedido solo por votación**; matar, final y otras reglas **configurables**; **espectadores en tercera persona** sin micrófono con los vivos; IAs que **suben de dificultad**; plan con **IAs y solitario primero**. |
| **0.4** | **4 a 8 tareas en total**, cooperativas y divididas en **sub-objetivos**; **estrellas por contribución individual**; se acepta el ritmo sin director (si nadie sale, sube la dificultad) y la duración estimada. |

---

## 21. Contexto para continuar en otra sesión

Esta sección resume lo que hay que saber para retomar el proyecto sin haber visto la conversación anterior.

### El proyecto de Unreal (estado al escribir este documento)

- **Carpeta:** `C:\Users\Nico\Documents\Unreal Projects\UnpaidOvertime`.
- **Versión del motor:** el `.uproject` indica **5.8**.
- **Lenguaje:** C++ (con módulo `UnpaidOvertime`). Plantilla de **primera persona**, con las variantes **Horror** y **Shooter** incluidas.
- **Plugins activos en el `.uproject`:** `ModelingToolsEditorMode`, `StateTree`, `GameplayStateTree` y `VisualStudioTools`.
- **No están activados** el *Python Editor Script Plugin* ni el *Remote Control API*, que los servidores MCP necesitan.
- **No existe** `Config/DefaultRemoteControl.ini`.
- **La carpeta no es un repositorio Git** todavía.

### Herramientas y flujo de trabajo

- **Visual Studio 2022** para **compilar y depurar** el C++ de Unreal. **No existe una extensión oficial de Claude Code para Visual Studio 2022** (las integraciones oficiales son VS Code y JetBrains).
- **Claude Code** se usa desde **VS Code** (con la carpeta del proyecto de Unreal abierta) o desde la **terminal** con `claude`, ejecutado en esa carpeta.
- Al agregar archivos `.cpp` o `.h` nuevos hay que **regenerar los archivos de proyecto de Visual Studio** (click derecho sobre el `.uproject`).
- Los **Blueprints, niveles y materiales** son binarios y **no se editan como texto**: conviene escribir la lógica de juego en **C++** y usar Blueprints solo para lo visual.

### Servidores MCP (conexión con el editor)

| Servidor | Estado |
|---|---|
| **UEMCP** | Instalado. Reportado para UE 5.0 a 5.6. |
| **unreal-engine-mcp** | Instalado. Usa Remote Control API y Python dentro del editor. |
| **ChiR24 unreal-mcp** | Pendiente de instalar. Reportado para UE 5.0 a 5.7. |
| **runreal/unreal-mcp** | **Descartado**: la dirección dio 404 y no se verificó que exista. |

**Advertencias**
- Son proyectos **de la comunidad**, no oficiales. Los comandos de instalación **no se verificaron**: confirmarlos en el README de cada uno.
- Conviene usar **uno a la vez**, porque exponen herramientas parecidas.
- **Puede que no funcionen con UE 5.8**, porque los rangos de compatibilidad encontrados llegan a 5.6 y 5.7. Se sabrá al probar.
- Ejecutar Python de forma remota en el editor equivale a **ejecutar código en la máquina**: usarlo **solo en local**, sin exponerlo a la red.
- Instalar no es lo mismo que **registrar** el servidor en Claude Code (`claude mcp add ...`), y las herramientas aparecen en una **sesión nueva** abierta en la carpeta del proyecto.

### La versión web (respaldo y fuente de ideas)

- **Carpeta:** `C:\Users\Nico\Desktop\visual-office`.
- Es una **oficina virtual multijugador en 3D para el navegador** (Three.js, Colyseus, LiveKit, Clerk, Neon). Quedó **comiteada como última versión web**.
- Contiene ideas reutilizables: interacción por apuntado, sentarse, emotes, estados de presencia, personalización del personaje, salas y plantillas de estructura, pruebas de extremo a extremo y la filosofía de **servidor autoritativo**.
- **Su servidor no se reutiliza** en este juego: Steam gestiona identidad y sesiones.

### Reglas de trabajo acordadas con el usuario

- Las respuestas al usuario van **en español**. Los artefactos técnicos (código, comentarios, mensajes de commit) en **inglés**, salvo que se pida otra cosa.
- **Cuando el usuario dice "haz todo", se hace todo de corrido**, sin dividir la tarea ni frenar a preguntar entre partes.
- Commits **convencionales y sin atribución de IA**. El usuario hace los commits y los empujes.
- **No leer ni modificar archivos `.env*`.**
- Ante algo que no se pudo verificar, **decirlo con claridad**.

### Lo que falta decidir (resumen)

Ver las secciones **0.1** (interpretaciones) y **16** (decisiones abiertas). Las tareas ya están resueltas (en total, cooperativas, v0.4); lo siguiente es probar el reparto de sub-objetivos y su peso.

### Siguiente paso concreto

**Hito M0:**
1. Crear el repositorio Git, con `.gitignore` de Unreal (que excluya `Binaries/`, `Intermediate/`, `Saved/` y `DerivedDataCache/`) y **Git LFS** para los assets pesados.
2. Escribir un `CLAUDE.md` con las reglas del proyecto y la arquitectura de la sección 11.
3. Activar el *Python Editor Script Plugin* y el *Remote Control API*, registrar el servidor MCP y probarlo con una tarea simple (crear un actor y sacar una captura).
4. Después, **M1 y M2**: el prototipo de **1 jugador + 3 IAs** de un día, midiendo cuánto dura de verdad.

---

*Este documento es una hipótesis de diseño, no una garantía de éxito comercial. Los tiempos y las reglas deben validarse con jugadores reales.*
