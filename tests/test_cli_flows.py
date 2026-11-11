"""Smoke tests de uso real do terminal. Cada caso inicia o executavel com dados isolados."""
import pathlib
import subprocess
import tempfile
import unittest
from datetime import datetime
import os

EXE = pathlib.Path(os.environ.get("LABRESERVA_BIN", "/tmp/labreserva")).resolve()
DATE = "18/06/2040"
WEEK = "25/06/2040"
NEXT = "02/07/2040"

def session(folder, commands, expect_exit=True):
    args = [str(x) for x in commands]
    proc = subprocess.run([str(EXE)], input="\n".join(args) + "\n", cwd=folder,
                          text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                          timeout=15)
    out = proc.stdout
    assert proc.returncode == 0, (proc.returncode, out[-2500:], proc.stderr)
    assert "Entrada encerrada." not in out, ("Missing terminal input", out[-2500:])
    if expect_exit:
        assert "Ate logo!" in out, ("Didn't return normally to main menu", out[-2500:])
    return out

def reserve(folder, date=DATE, turn=1, students=25, lab=1, name="Ana",
            equips=(), weeks=0, confirmation=1):
    inp = ["1", "1", date, turn, students, lab, name, "01", "Fisica"]
    if equips:
        inp.append("s")
        for equip_id, qty in equips:
            inp.extend([equip_id, qty])
        inp.append("0")
    else:
        inp.append("n")
    inp += [weeks, confirmation, "0", "0"]
    return session(folder, inp)

class TerminalFlows(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.d = self.tmp.name

    def check(self, output, *needles):
        for needle in needles:
            self.assertIn(needle, output, output[-3500:])

    def test_01_first_boot_menus_reports(self):
        out = session(self.d, ["2", "2", "0", "3", "2", "0", "4", "4", "0", "0"])
        self.check(out, "Lab Informatica 1", "Lab Eletronica", "Projetor", "Total de reservas: 0")
        self.assertTrue((pathlib.Path(self.d) / "reservas_lab.dat").exists())

    def test_02_create_reopen_list_consult_agenda_report(self):
        self.check(reserve(self.d, name="Carlos"), "1 reserva(s) confirmada(s)", "Reserva 1")
        out = session(self.d, ["1", "2", "3", "1", "0",
                               "4", "1", DATE, "2", "Carlos", "0",
                               "4", "4", "0", "0"])
        self.check(out, "Carlos", "Matricula" if "Matricula" in out else "matricula",
                   "Turno     : Manha", "Total de reservas: 1", "Lab Informatica 1")
        self.assertEqual(out.count("Reserva nao encontrada"), 0)

    def test_03_invalid_date_past_and_escape(self):
        out = session(self.d, ["1", "1", "31/02/2040", "18/06/2000", "0", "0", "0"])
        self.check(out, "Data invalida", "data que ja passou", "Cadastro interrompido")
        out = session(self.d, ["1", "2", "0", "0"])
        self.check(out, "Nenhuma reserva encontrada")

    def test_04_exceed_max_capacity_recover(self):
        out = session(self.d, ["1", "1", DATE, "1", "40", "1", "25", "1",
                               "Carlos", "42", "Fisica", "n", "0", "1", "0", "0"])
        self.check(out, "Nenhum laboratorio comporta 40 alunos", "Maior capacidade: 30",
                   "1 reserva(s) confirmada(s)")

    def test_05_day_fully_booked_detected_immediately(self):
        for turn in (1, 2, 3):
            for lab, cap in ((1, 30), (2, 20), (3, 25), (4, 16)):
                self.check(reserve(self.d, turn=turn, students=cap, lab=lab,
                                   name=f"Prof_{turn}_{lab}"), "1 reserva(s) confirmada(s)")
        out = session(self.d, ["1", "1", DATE, "0", "0", "0"])
        self.check(out, "Nenhum laboratorio livre nessa data, em nenhum turno")

    def test_06_shift_fully_booked_choose_other_shift(self):
        for lab, cap in ((1, 30), (2, 20), (3, 25), (4, 16)):
            reserve(self.d, students=cap, lab=lab, name=f"Prof_{lab}")
        out = session(self.d, ["1", "1", DATE, "1", "1", "3", "25", "1",
                               "Bia", "10", "Fisica", "n", "0", "1", "0", "0"])
        self.check(out, "Todos os laboratorios estao ocupados nesse turno",
                   "1 reserva(s) confirmada(s)", "Reserva 5")

    def test_07_only_small_labs_free_change_students(self):
        reserve(self.d, lab=1)
        reserve(self.d, lab=3)
        out = session(self.d, ["1", "1", DATE, "1", "25", "1", "20", "2",
                               "Bia", "10", "Fisica", "n", "0", "1", "0", "0"])
        self.check(out, "livres comportam no maximo 20 alunos",
                   "1 reserva(s) confirmada(s)")

    def test_08_same_lab_shift_conflict_other_lab_and_other_shift(self):
        reserve(self.d, lab=1)
        out = session(self.d, ["1", "1", DATE, "1", "25", "1", "3",
                               "Bia", "15", "Fisica", "n", "0", "1", "0", "0"])
        self.check(out, "Este laboratorio ja esta ocupado nesse turno", "Reserva 2")
        self.check(reserve(self.d, turn=2, lab=1, name="NoTurnoTarde"),
                   "1 reserva(s) confirmada(s)")

    def test_09_shared_equipment_stock_and_deletion(self):
        reserve(self.d, lab=1, equips=((1, 2),))
        out = session(self.d, ["1", "1", DATE, "1", "25", "3", "Bia", "12", "Fisica",
                               "s", "1", "2", "1", "0", "0", "1", "0", "0"])
        self.check(out, "Digite um numero entre 1 e 1", "1 reserva(s) confirmada(s)")
        out = session(self.d, ["4", "3", DATE, "1", "0", "0"])
        self.check(out, "0/3")
        out = session(self.d, ["1", "5", "1", "s", "0", "4", "3", DATE, "1", "0", "0"])
        self.check(out, "Reserva excluida", "2/3")
        out = session(self.d, ["1", "5", "2", "s", "0", "4", "3", DATE, "1", "0", "0"])
        self.check(out, "Reserva excluida", "3/3")

    def test_10_weekly_skip_lab_conflict(self):
        reserve(self.d, date=WEEK, lab=1, name="Conflito")
        out = reserve(self.d, date=DATE, lab=1, weeks=2, name="Semanal")
        self.check(out, "laboratorio indisponivel (ignorada)",
                   "2 reserva(s) confirmada(s)", DATE, NEXT)
        out = session(self.d, ["1", "2", "0", "0"])
        self.check(out, "Conflito", "Semanal")

    def test_11_weekly_skip_equipment_conflict(self):
        reserve(self.d, date=WEEK, lab=3, equips=((1, 3),))
        out = reserve(self.d, date=DATE, lab=1, weeks=2, equips=((1, 1),))
        self.check(out, "equipamentos insuficientes (ignorada)", "2 reserva(s) confirmada(s)")

    def test_12_weekly_skip_end_of_calendar(self):
        out = reserve(self.d, date="31/12/2100", lab=1, weeks=2)
        self.check(out, "data fora do limite permitido", "1 reserva(s) confirmada(s)")

    def test_13_edit_combined_date_shift_intermediate_conflict(self):
        reserve(self.d, date=WEEK, turn=1, lab=1, name="OcupanteDia")
        reserve(self.d, date=DATE, turn=3, lab=1, name="OcupanteTurno")
        reserve(self.d, date=DATE, turn=1, lab=1, name="Titular")
        out = session(self.d, ["1", "4", "3", "2", "1", WEEK, "2", "2", "3",
                               "1", "0", "0"])
        self.check(out, "Reserva atualizada")
        out = session(self.d, ["1", "3", "3", "0", "0"])
        self.check(out, WEEK, "Turno     : Noite", "Titular")

    def test_14_edit_invalid_then_fix(self):
        reserve(self.d)
        out = session(self.d, ["1", "4", "1", "2", "3", "40", "1",
                               "2", "3", "20", "1", "0", "0"])
        self.check(out, "comporta 30 alunos", "Corrija os problemas", "Reserva atualizada")
        out = session(self.d, ["1", "3", "1", "0", "0"])
        self.check(out, "Alunos    : 20")

    def test_15_equipment_edit_keep_cancel_remove(self):
        reserve(self.d, equips=((1, 2),))
        out = session(self.d, ["1", "4", "1", "2", "7", "0", "1", "0", "0"])
        self.check(out, "Reserva atualizada")
        out = session(self.d, ["1", "4", "1", "2", "7", "1", "-1", "1", "0", "0"])
        self.check(out, "Alteracao de equipamentos descartada", "Reserva atualizada")
        out = session(self.d, ["1", "3", "1", "0", "0"])
        self.check(out, "2x Projetor")
        out = session(self.d, ["1", "4", "1", "2", "7", "2", "1", "0", "0"])
        self.check(out, "Reserva atualizada")
        out = session(self.d, ["1", "3", "1", "0", "0"])
        self.check(out, "Equipamentos: nenhum")

    def test_16_delete_refused_then_accepted_and_new_id(self):
        reserve(self.d, equips=((1, 3),))
        out = session(self.d, ["1", "5", "1", "n", "0", "0"])
        self.check(out, "Excluir definitivamente esta reserva?")
        out = session(self.d, ["1", "3", "1", "0", "0"])
        self.check(out, "Professor")
        out = session(self.d, ["1", "5", "1", "s", "0", "0"])
        self.check(out, "Reserva excluida")
        out = session(self.d, ["1", "3", "1", "0", "0"])
        self.check(out, "Reserva nao encontrada")
        self.check(reserve(self.d, equips=((1, 3),)), "Reserva 2")

    def test_17_lab_cannot_shrink_below_existing_reservation(self):
        reserve(self.d, students=25)
        out = session(self.d, ["2", "3", "1", "Informatica A", "A", "Informatica",
                               "20", "25", "0", "0"])
        self.check(out, "Existem reservas futuras com ate 25 alunos",
                   "Digite um numero entre 25 e 500", "Laboratorio atualizado")

    def test_18_lab_equip_delete_blocked_if_booked(self):
        reserve(self.d, lab=1, equips=((1, 1),))
        out = session(self.d, ["2", "4", "1", "0", "3", "4", "1", "0", "0"])
        self.check(out, "ha reservas futuras neste laboratorio",
                   "ha reservas futuras com este equipamento")

    def test_19_equipment_stock_cannot_shrink_below_active_usage(self):
        reserve(self.d, equips=((1, 2),))
        out = session(self.d, ["3", "3", "1", "Projetor Novo", "Audiovisual",
                               "1", "2", "0", "0"])
        self.check(out, "Reservas futuras usam ate 2 unidades",
                   "Digite um numero entre 2 e 10000", "Equipamento atualizado")

    def test_20_crud_new_lab_and_equipment(self):
        out = session(self.d, ["2", "1", "Lab Novo", "D", "Tecnologia", "35",
                               "2", "5", "0", "0"])
        self.check(out, "Laboratorio cadastrado", "Lab Novo")
        out = session(self.d, ["3", "1", "Camera", "Audio", "8",
                               "2", "7", "0", "0"])
        self.check(out, "Equipamento cadastrado", "Camera")
        out = session(self.d, ["2", "4", "5", "s", "0", "3", "4", "7", "s", "0", "0"])
        self.check(out, "Laboratorio excluido", "Equipamento excluido")

    def test_21_corrupt_file_refused_without_overwrite(self):
        path = pathlib.Path(self.d) / "reservas_lab.dat"
        path.write_bytes(b"invalid binary file")
        old = path.read_bytes()
        proc = subprocess.run([str(EXE)], cwd=self.d, input="0\n", text=True,
                              capture_output=True, timeout=10)
        self.assertNotEqual(proc.returncode, 0)
        self.check(proc.stdout, "danificado ou e de outra versao")
        self.assertEqual(path.read_bytes(), old)

    def test_22_restore_backup_when_main_missing(self):
        session(self.d, ["0"])
        data = pathlib.Path(self.d) / "reservas_lab.dat"
        backup = pathlib.Path(self.d) / "reservas_lab.bak"
        backup.write_bytes(data.read_bytes())
        data.unlink()
        out = session(self.d, ["2", "2", "0", "0"])
        self.check(out, "Lab Informatica 1")
        self.assertTrue(data.exists())

if __name__ == "__main__":
    unittest.main(verbosity=2)
