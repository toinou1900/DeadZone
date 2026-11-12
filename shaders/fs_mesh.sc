$input v_normal

#include <bgfx_shader.sh>

uniform vec4 u_lightDir; // xyz : direction VERS la lumiere (repere du monde)
uniform vec4 u_color;    // couleur de base de la matiere

void main()
{
	vec3 n = normalize(v_normal);
	// eclairage a deux faces : le modele reste lisible meme vu "de dos"
	float ndl = abs(dot(n, normalize(u_lightDir.xyz)));
	vec3 col = u_color.rgb * (0.30 + 0.70 * ndl);
	gl_FragColor = vec4(col, 1.0);
}
