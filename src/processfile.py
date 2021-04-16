import pandas as pd
import xlsxwriter

def process_count(pdwriter, fine_path, worksheet, new_sheet_name, title):
    with open(fine_path, 'r') as temp_f:
        # get No of columns in each line
        col_count = [ len(l.split(" ")) for l in temp_f.readlines() ]
    print(col_count)
    ### Generate column names  (names will be 0, 1, 2, ..., maximum columns - 1)
    column_names = [i-2 for i in range(0, max(col_count))]
    column_names[0] = "data_size"
    column_names[1] = "count"
    print(column_names)

    #df = pd.read_csv(fine_path, usecols=[0,1], header=None, delim_whitespace=True, keep_default_na=True)
    df = pd.read_csv(fine_path, names=column_names, header=None, delim_whitespace=True, keep_default_na=True)
    #df.columns = ["data_size", "count"]
    # rowlen = df.shape[0]
    # print(df)
    # print(rowlen)
    mean_col = df.iloc[:,2:max(col_count)].mean(axis=1)
    df.insert(loc=2, column="avg.", value=mean_col)
    df.to_excel(pdwriter, sheet_name = new_sheet_name)

    # workbook  = pdwriter.book
    # worksheet = workbook.add_worksheet("Sheet2")
    # worksheet = pdwriter.sheets["Sheet2"]

    chart1 = workbook.add_chart({'type': 'column'})
    chart1.add_series({
        'name':       '= count',
        'categories': '= '+new_sheet_name+'!$B$2:$B$7',
        'values':     '= '+new_sheet_name+'!$C$2:$C$7',
        'data_labels': {'value': True},
    })
    chart1.set_title({'name': title})
    worksheet.insert_chart("D2", chart1)
    

pdwriter = pd.ExcelWriter('/home/xtang/mstatics/test/report.xlsx', engine='xlsxwriter') # pylint: disable=abstract-class-instantiated
workbook  = pdwriter.book
worksheet = workbook.add_worksheet("Sheet1")
process_count(pdwriter, "/home/xtang/mstatics/test/memset_latency.data", worksheet, "memset", "memset latency")
pdwriter.save()