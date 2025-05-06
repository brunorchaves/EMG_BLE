import matplotlib.pyplot as plt
import csv

CSV_FILENAME = 'thigh_20s_1.csv'

def read_adc_values(filename):
    values = []
    with open(filename, 'r') as file:
        reader = csv.reader(file)
        for row in reader:
            try:
                values.append(int(row[0]))
            except (ValueError, IndexError):
                pass
    return values

def plot_values(values):
    plt.figure()
    plt.plot(values)
    plt.title('Sinal ADC')
    plt.xlabel('Amostras')
    plt.ylabel('Valor ADC')
    plt.grid(True)
    plt.tight_layout()
    plt.savefig("adc_plot.png")
    print("Gráfico salvo como 'adc_plot.png'")

if __name__ == "__main__":
    adc_values = read_adc_values(CSV_FILENAME)
    if adc_values:
        plot_values(adc_values)
    else:
        print("Nenhum dado válido encontrado no arquivo.")
