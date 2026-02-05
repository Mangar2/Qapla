import re
import sys

def rescale_pst(text, mg_scale, eg_scale):
    # Basis-Skalierung (500 entspricht 100%)
    BASE = 500
    
    # Regex für Paare wie { 50,  0 } oder { -100, -50 }
    pattern = r"\{\s*(-?\d+)\s*,\s*(-?\d+)\s*\}"
    
    def replacer(match):
        mg = int(match.group(1))
        eg = int(match.group(2))
        
        # Berechnung (Rundung auf nächste Ganzzahl)
        new_mg = int(round(mg * mg_scale / BASE))
        new_eg = int(round(eg * eg_scale / BASE))
        
        # Formatierung beibehalten (Padding für Spaltenoptik)
        return f"{{ {new_mg:>3}, {new_eg:>3} }}"

    # Ersetze alle Vorkommen
    result = re.sub(pattern, replacer, text)
    return result

if __name__ == "__main__":
    # Beispielwerte falls keine Argumente
    mg_s = 300
    eg_s = 500
    
    print(f"/* Rescaled with MG={mg_s}, EG={eg_s} */")
    
    # Hier den Block aus pst.h einfügen
    input_text = """
			{ { -100, -50 }, { -50, -30 }, { -40, -20 }, { -40, -10 } },
			{ {  -30, -40 }, { -20, -25 }, { -10, -10 }, {  -5,   2 } },
			{ {  -20, -30 }, {  -5, -10 }, {   2,  -2 }, {   5,  10 } },
			{ {  -10, -20 }, {   2,   0 }, {  15,   5 }, {  25,  15 } },
			{ {  -10, -20 }, {   5,  -5 }, {  20,   5 }, {  25,  15 } },
			{ {   -2, -30 }, {  10, -15 }, {  25,  -5 }, {  25,  10 } },
			{ {  -30, -40 }, { -10, -25 }, {   0, -25 }, {  10,   2 } },
			{ { -100, -50 }, { -40, -40 }, { -25, -25 }, { -10, -10 } }
    """
    
    print(rescale_pst(input_text, mg_s, eg_s))
