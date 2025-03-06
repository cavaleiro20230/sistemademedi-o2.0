# sistemademedi-o2.0
Sistema de Monitoramento de Água


![image](https://github.com/user-attachments/assets/61f62382-2a9c-429b-8eb6-7befaf961b95)



![image](https://github.com/user-attachments/assets/e0944bcf-ce6b-43e7-ab7e-9e034a2b6b2c)





## 1. Adição do Monitoramento da Caixa d'Água

### Principais Mudanças:

- **Nova aba dedicada**: Adicionei uma aba específica "Caixa d'Água" para visualização e controle detalhados
- **Representação visual**: Criei uma visualização gráfica da caixa com nível de água em tempo real
- **Sistema de alertas**: Implementei alertas para níveis baixo, médio e alto da caixa
- **Histórico de nível**: Adicionei um gráfico para acompanhar a variação do nível ao longo do tempo


### Funcionamento do Sensor:

No código Arduino, implementei o monitoramento usando um sensor ultrassônico HC-SR04 que:

- Emite ondas sonoras que refletem na superfície da água
- Mede o tempo que a onda leva para retornar
- Calcula a distância entre o sensor (topo da caixa) e a água
- Converte essa distância em porcentagem de preenchimento


```plaintext
// Atualiza o nível da caixa d'água usando o sensor ultrassônico
void updateTankLevel() {
  // Faz 3 leituras e calcula a média para maior precisão
  int distance = 0;
  for (int i = 0; i < 3; i++) {
    delay(50);
    distance += sonar.ping_cm();
  }
  distance /= 3;
  
  // Calcula o nível em porcentagem (invertido, pois a distância é do topo)
  tankLevel = 100.0 * (1.0 - ((float)distance / TANK_HEIGHT));
}
```

## 2. Sistema de Alertas Inteligentes

### Níveis de Alerta Configuráveis:

- **Nível Baixo (padrão: 20%)**: Alerta crítico, ativa LED vermelho e buzzer
- **Nível Médio (padrão: 50%)**: Alerta de atenção, ativa LED amarelo
- **Nível Alto (padrão: 90%)**: Notificação de caixa quase cheia


### Indicadores Visuais:

- Cores diferentes para cada nível (vermelho, amarelo, azul, verde)
- Badges com status (Crítico, Médio-Baixo, Médio-Alto, Cheio)
- Alertas na dashboard principal


```typescriptreact
// Determina o status da caixa d'água
const getTankStatus = () => {
  if (tankLevel <= tankAlertLow) return { status: "Baixo", color: "destructive" }
  if (tankLevel <= tankAlertMid) return { status: "Médio-Baixo", color: "warning" }
  if (tankLevel <= tankAlertHigh) return { status: "Médio-Alto", color: "default" }
  return { status: "Cheio", color: "success" }
}
```

## 3. Interface Visual Aprimorada

### Visualização da Caixa:

- **Representação gráfica**: Uma visualização da caixa com nível de água animado
- **Marcações de nível**: Linhas tracejadas indicando os níveis de alerta (20%, 50%, 90%)
- **Código de cores**: Cores diferentes para cada faixa de nível


### Dashboard Integrada:

- Adicionei um card de resumo da caixa d'água na dashboard principal
- Integrei alertas da caixa com os alertas existentes
- Criei uma barra de progresso para visualização rápida do nível


## 4. Configurações Personalizáveis

### Parâmetros Ajustáveis:

- **Capacidade da caixa**: Permite definir o volume total em litros
- **Níveis de alerta**: Sliders para ajustar os percentuais de alerta
- **Ativação/desativação**: Botão para ligar/desligar os alertas da caixa


```typescriptreact
<div className="space-y-4 mt-4">
  <div>
    <div className="flex justify-between mb-1">
      <span className="text-sm">Alerta de Nível Baixo: {tankAlertLow}%</span>
    </div>
    <Slider 
      value={[tankAlertLow]} 
      min={5} 
      max={40} 
      step={5}
      onValueChange={(value) => setTankAlertLow(value[0])}
      disabled={!tankAlertsEnabled}
    />
  </div>
  
  {/* Outros sliders... */}
</div>
```

## 5. Integração com o Sistema Existente

### Armazenamento de Dados:

- Adicionei novos endereços na EEPROM para armazenar:

- Capacidade da caixa
- Níveis de alerta
- Configurações do sensor





### Lógica de Alertas Combinada:

- O sistema agora verifica tanto vazamentos quanto níveis da caixa
- Os LEDs e buzzer são compartilhados, com prioridade para alertas críticos


```plaintext
// Verifica alertas baseados no nível da caixa e vazão
void checkAlerts() {
  // Reseta todos os LEDs
  digitalWrite(LED_GREEN, LOW);
  digitalWrite(LED_YELLOW, LOW);
  digitalWrite(LED_RED, LOW);
  noTone(BUZZER_PIN);
  
  // Verifica nível da caixa
  if (tankLevel <= tankAlertLow) {
    // Nível crítico
    digitalWrite(LED_RED, HIGH);
    if (tankAlertsEnabled) {
      tone(BUZZER_PIN, 2000, 500);
    }
  } else if (tankLevel <= tankAlertMid) {
    // Nível médio-baixo
    digitalWrite(LED_YELLOW, HIGH);
  } else {
    // Nível normal
    digitalWrite(LED_GREEN, HIGH);
  }
  
  // Se houver vazamento detectado, pisca o LED vermelho
  if (leakDetected) {
    if ((millis() / 500) % 2 == 0) {
      digitalWrite(LED_RED, HIGH);
      if (tankAlertsEnabled) {
        tone(BUZZER_PIN, 1500, 200);
      }
    }
  }
}
```

## 6. Benefícios das Mudanças

### Prevenção de Problemas:

- **Evita falta d'água**: Alertas antecipados quando o nível está baixo
- **Previne desperdício**: Notifica quando a caixa está quase cheia
- **Detecta vazamentos**: Monitora tanto o fluxo quanto o nível para identificar problemas


### Economia e Eficiência:

- **Otimização do uso**: Ajuda a gerenciar melhor o consumo
- **Planejamento**: Fornece dados para melhor planejamento do uso da água
- **Redução de custos**: Evita desperdícios e identifica problemas rapidamente


### Facilidade de Uso:

- **Interface intuitiva**: Visualização clara do nível da caixa
- **Alertas configuráveis**: Adaptáveis às necessidades do usuário
- **Monitoramento completo**: Sistema unificado para fluxo e nível


## 7. Implementação Técnica

### Hardware Adicional:

- **Sensor ultrassônico HC-SR04**: Para medir o nível da água
- **LED amarelo**: Indicador adicional para alertas de nível médio
- **Conexões adicionais**: Pinos para o sensor ultrassônico (TRIG e ECHO)


### Modificações no Software:

- Novas funções para leitura e processamento do nível
- Sistema de alertas expandido
- Interface de usuário aprimorada com novas telas
- Armazenamento de configurações adicionais na EEPROM


Estas mudanças transformam o sistema de um simples medidor de fluxo para uma solução completa de monitoramento de água, oferecendo muito mais controle e informações para o usuário.
