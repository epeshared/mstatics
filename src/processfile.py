import pandas as pd
import xlsxwriter

def process_latency(pdwriter, fine_path, chart_sheet, data_sheet_name, title):
    with open(fine_path, 'r') as temp_f:
        # get No of columns in each line
        col_count = [ len(l.split(" ")) for l in temp_f.readlines() ]
    # print(col_count)
    ### Generate column names  (names will be 0, 1, 2, ..., maximum columns - 1)
    column_names = [i-2 for i in range(0, max(col_count))]
    column_names[0] = "data_size"
    column_names[1] = "count"
    #print(column_names)

    #df = pd.read_csv(fine_path, usecols=[0,1], header=None, delim_whitespace=True, keep_default_na=True)
    df = pd.read_csv(fine_path, names=column_names, header=None, delim_whitespace=True, keep_default_na=True)
    #df.columns = ["data_size", "count"]
    # rowlen = df.shape[0]
    # print(df)
    # print(rowlen)

    rowlen = df.shape[0]
    collen = df.shape[1]

    mean_col = df.iloc[:,2:max(col_count)].mean(axis=1)
    df.insert(loc=2, column="avg.", value=mean_col)
    df = df.round({"avg.":0})
    df.to_excel(pdwriter, sheet_name = data_sheet_name)

    # workbook  = pdwriter.book
    # worksheet = workbook.add_worksheet("Sheet2")
    # worksheet = pdwriter.sheets["Sheet2"]

    chart1 = workbook.add_chart({'type': 'column'})
    chart1.add_series({
        'name':       '= count',
        'categories': '= '+data_sheet_name+'!$B$2:$B$7',
        'values':     '= '+data_sheet_name+'!$C$2:$C$7',
        'data_labels': {'value': True},
    })
    chart1.set_title({'name': title + " count"})
    chart_sheet.insert_chart("A2", chart1)

    chart2 = workbook.add_chart({'type': 'column'})
    chart2.add_series({
        'name':       '= avg. latency',
        'categories': '= '+data_sheet_name+'!$B$2:$B$7',
        'values':     '= '+data_sheet_name+'!$D$2:$D$7',
        'data_labels': {'value': True},
    })
    chart2.set_title({'name': title+" latency (us)"})
    chart_sheet.insert_chart("K2", chart2)

    data_sheet = workbook.get_worksheet_by_name(data_sheet_name)

    for index, row in df.iterrows():
        line = index + 1
        # print((row.values.tolist()))
        # print("line:"+str(line))
        linechart=workbook.add_chart({'type': 'line'})
        linechart.add_series({
            'name':       [data_sheet_name, line, 1],
            'categories': [data_sheet_name, line, 0, line, 0],
            'values':     [data_sheet_name, line, 4, line, collen],
        })
        chart_row = (rowlen + 2) + (index//3)*15
        char_col = 10*( index % 3)
        # print("insert row:" + str(chart_row))
        # print("insert col:" + str(char_col))
        data_sheet.insert_chart(chart_row, char_col, linechart)    
    return df    

def process_interval(pdwriter, fine_path, chart_sheet, data_sheet_name, title):
    with open(fine_path, 'r') as temp_f:
        # get No of columns in each line
        col_count = [ len(l.split(" ")) for l in temp_f.readlines() ]
    # print(col_count)
    ### Generate column names  (names will be 0, 1, 2, ..., maximum columns - 1)
    column_names = [i-2 for i in range(0, max(col_count))]
    column_names[0] = "data_size"
    column_names[1] = "count"
    # print(column_names)    
    df = pd.read_csv(fine_path, names=column_names, header=None, delim_whitespace=True, keep_default_na=True)
    # print(df)

    rowlen = df.shape[0]
    collen = df.shape[1]

    mean_col = df.iloc[:,2:max(col_count)].mean(axis=1)
    df.insert(loc=2, column="avg.", value=mean_col)
    df = df.round({"avg.":0})
    df.to_excel(pdwriter, sheet_name = data_sheet_name)    

    chart1=workbook.add_chart({'type': 'column'})
    chart1.add_series({
        'name':       '= avg. interval',
        'categories': '= '+data_sheet_name+'!$B$2:$B$7',
        'values':     '= '+data_sheet_name+'!$D$2:$D$7',
        'data_labels': {'value': True},
    })
    chart1.set_title({'name': title+" interval (us)"})
    chart_sheet.insert_chart("U2", chart1)

    data_sheet = workbook.get_worksheet_by_name(data_sheet_name)

    for index, row in df.iterrows():
        line = index + 1
        # print((row.values.tolist()))
        # print("line:"+str(line))
        linechart=workbook.add_chart({'type': 'line'})
        linechart.add_series({
            'name':       [data_sheet_name, line, 1],
            'categories': [data_sheet_name, line, 0, line, 0],
            'values':     [data_sheet_name, line, 4, line, collen],
        })
        chart_row = (rowlen + 2) + (index//3)*15
        char_col = 10*( index % 3)
        # print("insert row:" + str(chart_row))
        # print("insert col:" + str(char_col))
        data_sheet.insert_chart(chart_row, char_col, linechart)



pdwriter = pd.ExcelWriter('/home/xtang/mstatics/test/report.xlsx', engine='xlsxwriter') # pylint: disable=abstract-class-instantiated
workbook  = pdwriter.book
chart_sheet = workbook.add_worksheet("Sheet1")
process_latency(pdwriter, "/home/xtang/mstatics/test/memset_latency.data", chart_sheet, "memset_latency", "memset")
process_interval(pdwriter, "/home/xtang/mstatics/test/memset_interval.data", chart_sheet, "memset_interval", "memset")
pdwriter.save()