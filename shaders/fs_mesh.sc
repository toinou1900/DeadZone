$input v_normal, v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(s_texColor, 0); // texture de couleur (une texture blanche 1x1 si la matiere n'en a pas)

uniform vec4 u_lightDir; // xyz : direction VERS la lumiere (repere du monde)
uniform vec4 u_color;    // couleur de base de la matiere (multipliee par la texture)

void main()
{
	vec3 n = normalize(v_normal);
	// eclairage a deux faces : le modele reste lisible meme vu "de dos"
	float ndl = abs(dot(n, normalize(u_lightDir.xyz)));
	vec3 albedo = texture2D(s_texColor, v_texcoord0).rgb * u_color.rgb;
	gl_FragColor = vec4(albedo * (0.30 + 0.70 * ndl), 1.0);
}
