import math

def calcular_altura_teto(x_metros):
    altura_nominal = 10.0
    
    # === Bloco 1 de anomalias (0m - 50m) ===
    # Saliência retangular: m10 ao m12
    if 10.0 <= x_metros < 12.0:
        return 9.0
    # Buraco retangular: m25 ao m28
    if 25.0 <= x_metros < 28.0:
        return 11.2
    # Saliência suave (Curva Senoidal): m45 ao m50
    if 45.0 <= x_metros <= 50.0:
        progresso = (x_metros - 45.0) / (50.0 - 45.0)
        return 10.0 - 1.5 * math.sin(progresso * math.pi)
    
    # === Bloco 2 de anomalias (50m - 150m) ===
    # Saliência triangular: m60 ao m63
    if 60.0 <= x_metros <= 63.0:
        meio = 61.5
        if x_metros <= meio:
            return 10.0 - ((x_metros - 60.0) / (meio - 60.0))
        else:
            return 9.0 + ((x_metros - meio) / (63.0 - meio))
            
    # Buraco triangular: m84 ao m88
    if 84.0 <= x_metros <= 88.0:
        meio = 86.0
        if x_metros <= meio:
            return 10.0 + 1.3 * ((x_metros - 84.0) / (meio - 84.0))
        else:
            return 11.3 - 1.3 * ((x_metros - meio) / (88.0 - meio))
            
    # Buraco retangular: m134 ao m144
    if 134.0 <= x_metros < 144.0:
        return 11.8
        
    # === Bloco 3 de anomalias (150m - 250m) ===
    # Buraco retangular: m150 ao m155
    if 150.0 <= x_metros < 155.0:
        return 11.5
    # Ondulações rítmicas de concreto: m200 ao m220
    if 200.0 <= x_metros <= 220.0:
        # Simplificação matemática equivalente e contínua para i * 0.5
        return 10.0 - 1.2 * math.sin(x_metros * 5.0)
        
    return altura_nominal